# Small, scalable health-bar artwork. Source geometry stays editable here.
Add-Type -AssemblyName System.Drawing
$artDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Art/EnemyUI'))
[IO.Directory]::CreateDirectory($artDir) | Out-Null

function Color([string]$hex) { [Drawing.ColorTranslator]::FromHtml($hex) }
function Polygon($graphics, $brush, [float[]]$coords) {
    $points = for ($i = 0; $i -lt $coords.Length; $i += 2) { [Drawing.PointF]::new($coords[$i], $coords[$i + 1]) }
    $graphics.FillPolygon($brush, [Drawing.PointF[]]$points)
}
function Solid([string]$hex) { [Drawing.SolidBrush]::new((Color $hex)) }
function Save-Art($bitmap, [int]$width, [int]$height, [string]$name) {
    $small = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($small)
    $g.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($bitmap, [Drawing.Rectangle]::new(0, 0, $width, $height))
    $g.Dispose()
    $small.Save((Join-Path $artDir $name), [Drawing.Imaging.ImageFormat]::Png)
    $small.Dispose()
    $bitmap.Dispose()
}

$frame = [Drawing.Bitmap]::new(512, 64, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [Drawing.Graphics]::FromImage($frame)
$g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.ScaleTransform(4, 4)
Polygon $g (Solid '#050C10') @(0,8, 6,0, 122,0, 128,8, 122,16, 6,16)
$gold = [Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.RectangleF]::new(0,1,128,14), (Color '#E5C990'), (Color '#7D5934'), 90)
Polygon $g $gold @(1,8, 6.5,1.5, 121.5,1.5, 127,8, 121.5,14.5, 6.5,14.5)
Polygon $g (Solid '#101F26') @(3,8, 7,3, 121,3, 125,8, 121,13, 7,13)
$g.FillRectangle((Solid '#071015'), 8,4,112,8)
$g.DrawLine([Drawing.Pen]::new((Color '#304048'), .65), 8,11.5,120,11.5)
$g.DrawLine([Drawing.Pen]::new((Color '#F4DFAD'), .65), 8,2.3,120,2.3)
Polygon $g (Solid '#E1BF7B') @(2,8, 4,6, 6,8, 4,10)
Polygon $g (Solid '#E1BF7B') @(122,8, 124,6, 126,8, 124,10)
$gold.Dispose(); $g.Dispose()
Save-Art $frame 128 16 'HealthFrame.png'

$fill = [Drawing.Bitmap]::new(512, 32, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [Drawing.Graphics]::FromImage($fill)
$g.ScaleTransform(4,4)
$red = [Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.RectangleF]::new(0,0,128,8), (Color '#FB5446'), (Color '#9B1826'), 90)
$g.FillRectangle($red, 0,0,128,8)
$g.FillRectangle((Solid '#FF9270'), 0,0,128,1)
$g.FillRectangle((Solid '#661725'), 0,7,128,1)
$red.Dispose(); $g.Dispose()
Save-Art $fill 128 8 'HealthFill.png'
Write-Output "Health-bar artwork saved to $artDir"
