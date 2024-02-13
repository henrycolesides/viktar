# viktar
Viking tar (Viktar) was a class project for CS333: Intro to Operating Systems at Portland State University.
The assignment was to create an archiving tool, similar to tar, that operates on regular filetypes only in the C programming language.
Viktar only works on binary archives with the .viktar extension and format.

Viktar options:   
	-t:	Display a short table of contents for a givin .viktar binary archive.   
	-T:	Display a detailed table of contents for a givin .viktar binary archive.   
	-c:	Build a .viktar binary archive from any regular files provided on the command line.   
	-x:	Extract a givin .viktar binary archive. Default functionality is to extract the entire archive, overwriting existing files.   
		If any filenames are specified, only those will be extracted from the archive.   
	-f:	Specify a .viktar file name for operation on by the previous four options.   
		If -f [option] is not provided, viktar will default to stdin.   
	-h:	Provide use options for the viktar program.   
	-v:	Run program in verbose mode.   
