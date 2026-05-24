param(
    [int]$Port = 8765
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$listener = [System.Net.HttpListener]::new()
$listener.Prefixes.Add("http://localhost:$Port/")

function Get-ContentType {
    param([string]$Path)

    switch ([System.IO.Path]::GetExtension($Path).ToLowerInvariant()) {
        '.html' { 'text/html; charset=utf-8' }
        '.js' { 'application/javascript; charset=utf-8' }
        '.css' { 'text/css; charset=utf-8' }
        '.json' { 'application/json; charset=utf-8' }
        '.svg' { 'image/svg+xml' }
        '.png' { 'image/png' }
        default { 'application/octet-stream' }
    }
}

function Write-TextResponse {
    param(
        [System.Net.HttpListenerResponse]$Response,
        [int]$StatusCode,
        [string]$Body
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Body)
    $Response.StatusCode = $StatusCode
    $Response.ContentType = 'text/plain; charset=utf-8'
    $Response.ContentLength64 = $bytes.Length
    $Response.OutputStream.Write($bytes, 0, $bytes.Length)
}

try {
    $listener.Start()
    Write-Host "Serving arm dashboard on http://localhost:$Port/"
    Write-Host 'Press Ctrl+C to stop.'

    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $requestPath = $context.Request.Url.AbsolutePath.TrimStart('/')
        if ([string]::IsNullOrWhiteSpace($requestPath)) {
            $requestPath = 'index.html'
        }

        $fullPath = Join-Path $root $requestPath
        try {
            $resolvedRoot = [System.IO.Path]::GetFullPath($root)
            $resolvedPath = [System.IO.Path]::GetFullPath($fullPath)
            if (-not $resolvedPath.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                Write-TextResponse -Response $context.Response -StatusCode 403 -Body 'Forbidden'
            }
            elseif (-not (Test-Path $resolvedPath -PathType Leaf)) {
                Write-TextResponse -Response $context.Response -StatusCode 404 -Body 'Not found'
            }
            else {
                $bytes = [System.IO.File]::ReadAllBytes($resolvedPath)
                $context.Response.StatusCode = 200
                $context.Response.ContentType = Get-ContentType -Path $resolvedPath
                $context.Response.ContentLength64 = $bytes.Length
                $context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
            }
        }
        finally {
            $context.Response.OutputStream.Close()
        }
    }
}
finally {
    if ($listener.IsListening) {
        $listener.Stop()
    }
    $listener.Close()
}