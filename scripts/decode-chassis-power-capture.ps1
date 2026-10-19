param(
    [Parameter(Mandatory = $true)][string]$InputPath,
    [Parameter(Mandatory = $true)][string]$OutputPath
)
$ErrorActionPreference = 'Stop'
$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $InputPath))
$rows = [Collections.Generic.List[object]]::new()
$groups = @{}
$corrupt = 0
function Get-CaptureCrc([byte[]]$data, [int]$offset) {
    [uint32]$crc = 65535
    for ($i = 0; $i -lt 100; $i++) {
        $crc = $crc -bxor $data[$offset + $i]
        for ($bit = 0; $bit -lt 8; $bit++) {
            if ($crc -band 1) { $crc = ($crc -shr 1) -bxor 40961 }
            else { $crc = $crc -shr 1 }
        }
    }
    return [uint16]$crc
}
for ($offset = 0; $offset + 4 -le $bytes.Length; $offset++) {
    if ($bytes[$offset] -ne 67 -or $bytes[$offset + 1] -ne 80 -or
        $bytes[$offset + 2] -ne 67 -or $bytes[$offset + 3] -ne 49) { continue }
    if ($offset + 102 -gt $bytes.Length) { throw 'Truncated capture frame' }
    if ((Get-CaptureCrc $bytes $offset) -ne [BitConverter]::ToUInt16($bytes, $offset + 100)) {
        $corrupt++; continue
    }
    $p = $offset + 4
    if ($bytes[$p] -ne 1) { throw "Unsupported capture version $($bytes[$p])" }
    $id = [BitConverter]::ToUInt16($bytes, $p + 2)
    $seq = [BitConverter]::ToUInt16($bytes, $p + 4)
    if (!$groups.ContainsKey($id)) { $groups[$id] = @{ Records = @{}; Summary = $null } }
    $group = $groups[$id]
    if ($bytes[$p + 1] -eq 2) {
        if ($null -ne $group.Summary) { throw "Duplicate summary for capture $id" }
        $group.Summary = @{
            Sequence = $seq; Trigger = [BitConverter]::ToUInt32($bytes, $p + 8)
            Status = [BitConverter]::ToUInt32($bytes, $p + 16)
            T90 = [BitConverter]::ToUInt32($bytes, $p + 20)
            Dropped = [BitConverter]::ToUInt32($bytes, $p + 24)
            Count = [BitConverter]::ToUInt32($bytes, $p + 28)
        }
    } elseif ($bytes[$p + 1] -eq 1) {
        if ($group.Records.ContainsKey($seq)) { throw "Duplicate sequence $seq in capture $id" }
        $row = [ordered]@{
            capture_id = $id; sequence = $seq
            tick_ms = [BitConverter]::ToUInt32($bytes, $p + 8)
            meter_tick_ms = [BitConverter]::ToUInt32($bytes, $p + 12)
            state = $bytes[$p + 6]; flags = $bytes[$p + 7]
        }
        $names = @('raw_current', 'final_current', 'feedback_current', 'actual_rpm', 'target_rpm')
        $wheels = @('lf', 'rf', 'lb', 'rb')
        for ($a = 0; $a -lt 5; $a++) {
            for ($w = 0; $w -lt 4; $w++) {
                $row["$($names[$a])_$($wheels[$w])"] = [BitConverter]::ToInt16($bytes, $p + 16 + 8 * $a + 2 * $w)
            }
        }
        $scalars = @('budget_w','measured_w','actual_model_w','guard_model_w','fast_w','slow_w','excess_j','allowance_j')
        for ($i = 0; $i -lt 8; $i++) {
            $row[$scalars[$i]] = [BitConverter]::ToSingle($bytes, $p + 56 + 4 * $i).ToString('R', [Globalization.CultureInfo]::InvariantCulture)
        }
        for ($w = 0; $w -lt 4; $w++) { $row["motor_age_ms_$($wheels[$w])"] = $bytes[$p + 88 + $w] }
        $row.meter_age_ms = $bytes[$p + 92]; $row.enabled_mask = $bytes[$p + 93]
        $row.drive_mask = $bytes[$p + 94]; $row.brake_mask = $bytes[$p + 95]
        $group.Records[$seq] = $row
    } else { throw "Unsupported frame kind $($bytes[$p + 1])" }
    $offset += 101
}
if ($corrupt) { throw "$corrupt corrupt capture frame(s); CSV not exported" }
if ($groups.Count -eq 0) { throw 'No capture frames found' }
foreach ($id in ($groups.Keys | Sort-Object)) {
    $group = $groups[$id]; $summary = $group.Summary
    if ($null -eq $summary -or $summary.Count -ne $group.Records.Count -or
        $summary.Sequence -ne $summary.Count) { throw "Incomplete capture $id" }
    for ($seq = 0; $seq -lt $summary.Count; $seq++) {
        if (!$group.Records.ContainsKey([uint16]$seq)) { throw "Missing sequence $seq in capture $id" }
        $row = $group.Records[[uint16]$seq]
        $row.trigger_tick_ms = $summary.Trigger; $row.t90_status = $summary.Status
        $row.t90_ms = $summary.T90; $row.dropped_triggers = $summary.Dropped
        $rows.Add([pscustomobject]$row)
    }
}
$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding utf8
Write-Output "Decoded $($groups.Count) capture(s), $($rows.Count) records"
