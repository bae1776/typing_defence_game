typing_defence : document_picker.c document_picker.h simplehash.h typing_defence.c
	gcc -o ./typing_defence typing_defence.c document_picker.c -lcurses simplehash.h document_picker.h

clean :
	rm -f ./typing_defence
	rm -f ./wordList.txt
	rm -f ./manual.txt
	rm -f ./*.pch

run : typing_defence
	./typing_defence