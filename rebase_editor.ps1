(Get-Content $args[0]) -replace '^pick (.*Bump commit count.*)$', 'drop $1' | Set-Content $args[0]
