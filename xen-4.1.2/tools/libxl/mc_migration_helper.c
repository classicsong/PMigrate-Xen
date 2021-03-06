#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include "mc_migration_helper.h"

#define hprintf(_f, _a...) \
    if (mc_migrate_hint == 1) ( fprintf(stderr, _f, ## _a), fflush(stderr) )

/* Parse the Migration Destination IP File */
int parse_dest_file(char* dest_file, char*** dests, int* dest_cnt) 
{
	int store_cnt = BUFFER_INIT_SIZE, i = 0;
	char **ip_store = (char **)malloc(sizeof(char*) * BUFFER_INIT_SIZE);
	FILE *fd = fopen(dest_file, "r");
	if (!fd)
		return -1;

	while (1) {
		/* Break Only realloc the buffer */
		while (i < store_cnt) {
			char *buf = NULL;
			size_t buf_size = 0;
			if( getline(&buf, &buf_size, fd) < 0 ) {
				store_cnt = i;
				goto out;
			}
			buf[strlen(buf) - 1] = '\0';
			ip_store[i] = buf;
			i++;
		}
		ip_store = realloc(ip_store, store_cnt + BUFFER_INIT_SIZE);
		store_cnt += BUFFER_INIT_SIZE;
	}

out:
	*dests = ip_store;
	*dest_cnt = store_cnt;
	return 0;
}

/* Change the Rune Command */
int rune_add_ips(char** rune, char** dests, char ***ports, int dest_cnt, int port_cnt)
{
	int i, j, len = BUFFER_INIT_SIZE, current;
	char *a = malloc(BUFFER_INIT_SIZE);
	char port_cnt_str[10];
	bzero(a, BUFFER_INIT_SIZE);
	bzero(port_cnt_str, 10);

	/* Append port number */
	sprintf(port_cnt_str, "%d", port_cnt);
	a = strcat(a, " ");
	a = strcat(a, port_cnt_str);

	for (i = 0; i < dest_cnt; i++) {
		if ((current = strlen(dests[i]) + strlen(a) + 1) > len){
			len = current * 2;
			a = realloc(a, len);
		}
		a = strcat(a, " ");
		a = strcat(a, dests[i]);
		for (j = 0; j < port_cnt; j++) {
			a = strcat(a, " ");
			a = strcat(a, ports[i][j]);
		}
	}

	asprintf(rune, "%s %s", *rune, a);
	return 0;
}

/* Network Socket Connection */
int mc_net_server(char *ip, char *port) 
{
	int sock, connect, addr_len;
	struct sockaddr_in server_addr,client_addr;
	struct hostent *host;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Create Socket Error\n");
		return -1;
	}

	if ((host = gethostbyname(ip)) == NULL) {
		fprintf(stderr, "Get Host By Ip Error\n");
		return -1;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(port));
	server_addr.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
	bzero(&(server_addr.sin_zero),8);

	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) != 0) {
		fprintf(stderr, "Bind Error: %s\n", strerror(errno));
		return -1;
	}
	
	pthread_mutex_lock(&receive_ready_banner.mutex);
	receive_ready_banner.cnt++;
	pthread_mutex_unlock(&receive_ready_banner.mutex);

	if (listen(sock, 10) == -1) {
		fprintf(stderr, "Listen Error\n");
		return -1;
	}

	addr_len = sizeof(client_addr);
	if ((connect = accept(sock, (struct sockaddr *) &client_addr, (socklen_t*) &addr_len)) < 0) {
		fprintf(stderr, "Accept Error: %s\n", strerror(errno));
	}
	return connect;
}

int mc_net_client(char *ip, char *port)
{
	struct hostent *host;
	struct sockaddr_in server_addr;  
	int sock;
	int i;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
	}

	if ((host = gethostbyname(ip)) == NULL) {
		return -1;
	}

	server_addr.sin_family = AF_INET;     
	server_addr.sin_port = htons(atoi(port));   
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server_addr.sin_zero),8);

	/* Need to connect multi times */
	for (i = 0; i < MULTI_TRY; i++) {
		if (connect(sock, (struct sockaddr *)&server_addr,
					sizeof(struct sockaddr)) == -1) 
		{
			usleep(1000);
			continue;
		}
		return sock;
	}

	return -1;
}

void init_banner(banner_t *banner, int count)
{
	banner->cnt = 0;
	pthread_mutex_init(&banner->mutex, NULL);
	if (count > 0) {
		pthread_barrier_init(&banner->barr, NULL, count);
	}
}

int init_list_head(struct list_item *list_head)
{
	list_head->item = NULL;
	list_head->next = list_head;
	list_head->prev = list_head;
	return 0;
}

// Push it as first item
int send_argu_enqueue(send_argu_t* argu)
{
	struct list_item *item;
	
	pthread_mutex_lock(&send_argu_head_mutex);
	item = (struct list_item*)malloc(sizeof(struct list_item));
	item->item = argu;

	item->next = send_argu_head->next;
	item->prev = send_argu_head;
	item->next->prev = item;
	send_argu_head->next = item;
	send_argu_head_cnt++;
	pthread_mutex_unlock(&send_argu_head_mutex);

	return 0;
}

int send_argu_dequeue(send_argu_t **argu) 
{
	struct list_item *item;

	pthread_mutex_lock(&send_argu_head_mutex);
	if (send_argu_head->next == send_argu_head) {
		pthread_mutex_unlock(&send_argu_head_mutex);
		return -1;
	}
	item = send_argu_head->prev;
	*argu = (send_argu_t*)item->item; 
	send_argu_head->prev= item->prev;
	send_argu_head->prev->next = send_argu_head;
	free(item);
	send_argu_head_cnt--;
	pthread_mutex_unlock(&send_argu_head_mutex);

	return 0;
}

int recv_pagebuf_enqueue(pagebuf_t *pagebuf)
{
	struct list_item *item;
	
	pthread_mutex_lock(&recv_pagebuf_head_mutex);
	item = (struct list_item*)malloc(sizeof(struct list_item));
	item->item = pagebuf;

	item->next = recv_pagebuf_head->next;
	item->prev = recv_pagebuf_head;
	item->next->prev = item;
	recv_pagebuf_head->next = item;
	pthread_mutex_unlock(&recv_pagebuf_head_mutex);

	return 0;
}

int recv_pagebuf_dequeue(pagebuf_t **pagebuf) 
{
	struct list_item *item;

	pthread_mutex_lock(&recv_pagebuf_head_mutex);
	if (recv_pagebuf_head->next == recv_pagebuf_head) {
		pthread_mutex_unlock(&recv_pagebuf_head_mutex);
		return -1;
	}
	item = recv_pagebuf_head->prev;
	*pagebuf = (pagebuf_t *)item->item; 
	recv_pagebuf_head->prev = item->prev;
	recv_pagebuf_head->prev->next = recv_pagebuf_head;
	free(item);
	pthread_mutex_unlock(&recv_pagebuf_head_mutex);

	return 0;
}

int apply_enqueue(top_to_buttom_t *argu)
{
	struct list_item *item;
	
	pthread_mutex_lock(&apply_queue_mutex);
	item = (struct list_item*)malloc(sizeof(struct list_item));
	item->item = argu;

	item->next = apply_queue_head->next;
	item->prev = apply_queue_head;
	item->next->prev = item;
	apply_queue_head->next = item;
	pthread_mutex_unlock(&apply_queue_mutex);

	return 0;
}

int apply_dequeue(top_to_buttom_t **argu)
{
	struct list_item *item;

	pthread_mutex_lock(&apply_queue_mutex);
	if (apply_queue_head->next == apply_queue_head) {
		pthread_mutex_unlock(&apply_queue_mutex);
		return -1;
	}
	item = apply_queue_head->prev;
	*argu= (top_to_buttom_t *)item->item; 
	apply_queue_head->prev = item->prev;
	apply_queue_head->prev->next = apply_queue_head;
	free(item);
	pthread_mutex_unlock(&apply_queue_mutex);

	return 0;
}

int pagebuf_pool_enqueue(pagebuf_t *pagebuf)
{
	struct list_item *item;
	
	pthread_mutex_lock(&pagebuf_pool_mutex);
	item = (struct list_item*)malloc(sizeof(struct list_item));
	item->item = pagebuf;

	item->next = pagebuf_pool->next;
	item->prev = pagebuf_pool;
	item->next->prev = item;
	pagebuf_pool->next = item;
	pthread_mutex_unlock(&pagebuf_pool_mutex);

	return 0;
}

int pagebuf_pool_dequeue(pagebuf_t **pagebuf) 
{
	struct list_item *item;

	pthread_mutex_lock(&pagebuf_pool_mutex);
	if (pagebuf_pool->next == pagebuf_pool) {
		pthread_mutex_unlock(&pagebuf_pool_mutex);
		return -1;
	}
	item = pagebuf_pool->prev;
	*pagebuf = (pagebuf_t *)item->item; 
	pagebuf_pool->prev = item->prev;
	pagebuf_pool->prev->next = pagebuf_pool;
	free(item);
	pthread_mutex_unlock(&pagebuf_pool_mutex);

	return 0;
}

