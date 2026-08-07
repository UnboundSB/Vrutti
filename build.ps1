if (!(Test-Path build)) { New-Item -ItemType Directory -Force -Path build }
$srcs = (Get-ChildItem -Path src -Filter *.cpp -Recurse | Where-Object { $_.FullName -notmatch 'vendor' }).FullName
$srcs_str = $srcs -join ' '
Invoke-Expression "g++ -std=c++20 -Wall -Wextra -O2 -I./src $srcs_str -o build/vrutti_app.exe -lole32 -lcomctl32 -loleaut32 -luuid -lgdi32 -lshlwapi -static -lws2_32 -lkernel32 -ladvapi32"
