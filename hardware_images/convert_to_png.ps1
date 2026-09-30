$edgePath = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
if (-not (Test-Path $edgePath)) {
    $edgePath = "C:\Program Files\Microsoft\Edge\Application\msedge.exe"
}

Write-Host "Found Edge at: $edgePath"

$items = @(
    @{ Svg = "central_hub_exact_schematic.svg"; Png = "central_hub_exact_wiring.png" },
    @{ Svg = "cot_node_exact_schematic.svg"; Png = "cot_node_exact_wiring.png" },
    @{ Svg = "wearable_exact_schematic.svg"; Png = "wearable_exact_wiring.png" }
)

$baseDir = "C:\Users\HP\Documents\Antigravity\Multi-node\hardware_images"

foreach ($item in $items) {
    $svgUrl = "file:///$baseDir/" + $item.Svg -replace '\\', '/'
    $outPng = Join-Path $baseDir $item.Png
    Write-Host "Rendering $svgUrl -> $outPng ..."
    & $edgePath --headless --disable-gpu --screenshot="$outPng" --window-size=1280,980 "$svgUrl"
}

Write-Host "All conversions completed!"
