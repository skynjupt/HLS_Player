#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define URL_LEN 256
#define PATH_LEN 256
#define TEST 1
char g_ip[URL_LEN+1];
char g_port[5+1];
char g_host[URL_LEN+5+1];

static char g_buf_send[4*1024];
static char g_buf_recv[10*1024];

void package_url(char *path)
{
	memset(g_buf_send, 0x0, sizeof(g_buf_send));
	sprintf(g_buf_send, "GET %s", path);
	
	strcat(g_buf_send, " HTTP/1.1\r\n");
	strcat(g_buf_send, "Host: ");
	strcat(g_buf_send, g_host);
	strcat(g_buf_send, "\r\nUser-Agent: OS 4_1");
	strcat(g_buf_send, "\r\nKeep-Alive: 400");
	strcat(g_buf_send, "\r\nConnection: Keep-Alive\r\n\r\n");
}

int http_get_content_length(char *revbuf)
{
	char *p1 = NULL, *p2 = NULL;
	int http_body = 0;
	
	p1 = strstr(revbuf, "Content-Length");
	if(p1 == NULL)
	{
		return -2;
	}
	else
	{
		p2 = p1 + strlen("Content-Length") + 2;
		http_body = atoi(p2);
		return http_body;
	}
}

int hls_send(int sockfd, char *sendbuf, int len, int flags)
{
	int sendlen = 0;
	int ret = -1;
	while(sendlen < len)
	{
		ret = send(sockfd, sendbuf+sendlen, len-sendlen, flags);
		if(-1 == ret)
		{
			printf("send error\n");
			return -1;
		}
		else
		{
			sendlen += ret;
		}
	}
	return 0;
}

int hls_recv(int sockfd, char *buf_recv, int *total_length)
{
	int ret;
	int recvlen = 0;
	int downloadlen = 0;
	char *buf_recv_tmp = NULL;
	
	buf_recv_tmp = malloc(1024*10+1);
	if(buf_recv_tmp == NULL)
	{
		printf("HTTP_Recv: sk_mem_malloc fail\n");
		return -1;
	}
	memset(buf_recv_tmp, 0x0, 1024*10+1);
	ret = recv(sockfd, buf_recv_tmp, 1024*10, 0);
	if(ret <= 0)
	{
		free(buf_recv_tmp);
		return ret;
	}
	if((*total_length) == 0)
	{
		downloadlen = http_get_content_length(buf_recv_tmp);
		if(downloadlen > 0)
		{
			(*total_length) = downloadlen;
		} 
	}
	recvlen += ret;
	memcpy(buf_recv, buf_recv_tmp, recvlen);
	free(buf_recv_tmp);
	return recvlen;
}

int http_get_file(int sockfd, char *path, char **pfilebuf)
{
	int total_recv = -1;
	int total_length = 0;
	int ret = -1;
	int b_head_get = 0;
	char *p = NULL;

	package_url(path);
#ifdef DEBUG_HTTP
	printf("send = %s \n", g_buf_send);
#endif
	hls_send(sockfd, g_buf_send, strlen(g_buf_send), 0);
	while(total_recv < total_length)
	{
#ifdef DEBUG_HTTP
		printf("---downloading---\n");
#endif
		memset(g_buf_recv, 0x0, sizeof(g_buf_recv));
		ret = hls_recv(sockfd, g_buf_recv, &total_length);
		if((*pfilebuf) == NULL && total_length > 0)
		{
			(*pfilebuf) = malloc(total_length);
			if((*pfilebuf) == NULL)
			{
				return -1;
			}
		}
		if(ret < 0)
		{
			break;
		}
		if(ret == 0)
		{
			close(sockfd);
			sockfd = socket_connect(g_ip, g_port);
			if(sockfd == -1)
			{
				return -1;
			}
			continue;
		}	

#ifdef DEBUG_HTTP
		printf("%s\n", g_buf_recv);
#endif
		if(b_head_get == 0)
		{
			p = strstr(g_buf_recv, "\r\n\r\n");
		}
		else
		{
			p = NULL;
		}

		if(p == NULL)
		{
			
			if(total_recv == -1)
			{
				total_recv = 0;
			};
			if((total_recv+ret) > total_length)
			{
				memcpy((*pfilebuf)+total_recv, g_buf_recv, total_length-total_recv);
				total_recv = total_length;
			}
			else
			{
				memcpy((*pfilebuf)+total_recv, g_buf_recv, ret);
				total_recv += ret;
			}	
		}
		else
		{
			b_head_get = 1;
			if(total_recv == -1)
			{
				total_recv = 0;
			};
			if((total_recv+(ret-(p-g_buf_recv+4))) > total_length)
			{
				memcpy((*pfilebuf)+total_recv,p+4, total_length-total_recv);
				total_recv = total_length;
			}
			else
			{
				memcpy((*pfilebuf)+total_recv,p+4, ret-(p-g_buf_recv+4));
				total_recv += ret-(p-g_buf_recv+4);
			}	
		}	
	}
	if(total_recv != total_length)
	{
		return -1;
	}
	return total_recv;
}

int socket_connect(char *ip, char *port)
{
	struct sockaddr_in addr;
	struct timeval connect_time;
	int len;
	int sockfd;
	
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(atoi(port));
	len = sizeof(addr);

	connect_time.tv_sec = 5;
	connect_time.tv_usec = 0;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&connect_time, sizeof(connect_time));
	if(connect(sockfd, (struct sockaddr *)&addr, len) != 0)
	{
		close(sockfd);
		return -1;
	}
	return sockfd;
}
//int http_download_file(char *url)
int http_download_file()
{
	int sockfd = -1;
	char path[PATH_LEN+1];	
	char *filebuf = NULL;
	char *p = NULL;
	char *p_end = NULL;
	int ret;
	FILE *fp = NULL;
 
	memset(g_ip,0x0,sizeof(g_ip));
	memset(g_port, 0x0, sizeof(g_port));
	memset(g_host, 0x0, sizeof(g_host));
	memset(path, 0x0, sizeof(path));

	sprintf(g_ip, "%s", "46.20.4.42");
	sprintf(g_port, "%s", "80");
	sprintf(g_host, "%s:%s", g_ip, g_port);
	sprintf(path, "%s", "/powerturk/powerturk1/playlist.m3u8");

	sockfd = socket_connect(g_ip, g_port);
	if(sockfd == -1)
	{
		return -1;
	}
	ret = http_get_file(sockfd, path, &filebuf);	
	close(sockfd);
	
	if(ret < 0)
	{
#ifdef DEBUG_HTTP
		printf("----Fail download----\n");
#endif

		if(filebuf != NULL)
		{
			free(filebuf);
			filebuf = NULL;
		}
		return -1;
	}
	else
	{
		if(filebuf != NULL)
		{
			// to do save file
#ifdef DEBUG_HTTP
			printf("---to do save file----\n");
#endif
			fp = fopen("playlist.m3u8", "w");
			if(fp == NULL)
			{
				printf("Error:fopen\n");
				free(filebuf);
				filebuf = NULL;
				return -1;
			}			
			
			p = filebuf;
			p_end = filebuf + ret;
			while(p < p_end)
			{
				fwrite(p, 1, 1, fp);
				p++;
			}
			fclose(fp);
			fp = NULL;
			free(filebuf);
			filebuf = NULL;
		}
	}
	return 1;
}

#if TEST
int main(int argc, char *argv[])
{
	http_download_file();
	return 0;
}
#endif
