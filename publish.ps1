# Publishes this folder to a new public GitHub repo (Windows).
# Run:  powershell -ExecutionPolicy Bypass -File publish.ps1 [repo-name]
param([string]$Name = "afo-q-4")
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Write-Host "Install git first: https://git-scm.com/downloads"; exit 1 }
if (-not (Test-Path .git)) { git init -b main }
git add -A
git commit -m "Afo-Q 4"
if (Get-Command gh -ErrorAction SilentlyContinue) {
  gh auth status 2>$null; if ($LASTEXITCODE -ne 0) { gh auth login }
  gh repo create $Name --public --source . --remote origin --push
} else {
  Write-Host "GitHub CLI (gh) not found. Create an EMPTY public repo named $Name on github.com, then run:"
  Write-Host "  git remote add origin https://github.com/<your-username>/$Name.git"
  Write-Host "  git push -u origin main"
}
Write-Host "Next: Settings > Pages > Source = GitHub Actions, then: git tag v0.1.0; git push origin v0.1.0"
