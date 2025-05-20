//%attributes = {}
$files:=Folder:C1567("Macintosh HD:Users:miyako:Desktop:soci:vcpkg:installed:arm64-osx:lib:"; fk platform path:K87:2).files().query("extension == :1"; ".a")

For each ($file; $files)
	
	$command:="lipo -create \""+$file.path+"\" \""+$file.parent.parent.parent.file("x64-osx/lib/"+$file.fullName).path+"\" -output \""+$file.parent.parent.parent.file($file.fullName).path+"\""
	LAUNCH EXTERNAL PROCESS:C811($command)
	
End for each 

OB Keys:C1719