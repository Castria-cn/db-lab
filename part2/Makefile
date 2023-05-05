cc: cc.l cc.y cc.h set.c set.h main.c
	bison -d cc.y
	flex -o cc.lex.c cc.l
	gcc -o $@ cc.tab.c cc.lex.c set.c main.c
clean:
	rm -rf cc.tab.c
	rm -rf cc.lex.c
	rm -rf cc.tab.h
	rm -rf cc
