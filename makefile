bpe: main.c stb_ds.h
	cc main.c -o bpe -ggdb

stb_ds.h:
	wget https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_ds.h
