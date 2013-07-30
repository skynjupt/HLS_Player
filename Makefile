hls_player:hls_http.o
	gcc -o $@ $<

hls_http.o:hls_http.c
	gcc -c -o $@ $< -DDEBUG_HTTP

.PHONY:clean

clean:
	-rm hls_player hls_http.o
