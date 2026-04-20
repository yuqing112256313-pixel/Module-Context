param(
    [ValidateSet('all', 'task-flow', 'perf')]
    [string]$Suite = 'all',
    [string]$BuildDir = '',
    [string]$RabbitMqEnv = 'external',
    [string]$RabbitMqApiUrl = 'http://127.0.0.1:15672/api',
    [string]$RabbitMqAdminUser = 'guest',
    [string]$RabbitMqAdminPass = 'guest'
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..\..')
$bashScript = Join-Path $scriptDir 'run_integration_tests.sh'

$env:MC_RABBITMQ_ENV = $RabbitMqEnv
$env:RABBITMQ_API_URL = $RabbitMqApiUrl
$env:RABBITMQ_ADMIN_USER = $RabbitMqAdminUser
$env:RABBITMQ_ADMIN_PASS = $RabbitMqAdminPass
if ($BuildDir -ne '') {
    $env:MC_BUILD_DIR = $BuildDir
}

Write-Host "[run] repo root: $repoRoot"
Write-Host "[run] suite: $Suite"
Write-Host "[run] MC_RABBITMQ_ENV=$($env:MC_RABBITMQ_ENV)"

if (Get-Command bash -ErrorAction SilentlyContinue) {
    & bash $bashScript $Suite
    exit $LASTEXITCODE
}

throw "bash not found in PATH. Install Git Bash and ensure bash.exe is available."
