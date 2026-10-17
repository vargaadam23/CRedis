#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "constants.h"
#include "clrf.h"

int main()
{
	Value *out = malloc(sizeof(struct Value));

	clrfToValue("$5\r\nhello\r\n", out);

	printf("%d - value type - %s - string", out->type, out->as.s);

	freeValue(out);

	printf("\n\n\n\n");

	out = malloc(sizeof(struct Value));

	clrfToValue(":1369\r\n", out);

	printf("%d - value type - %d - int", out->type, out->as.i);

	freeValue(out);

	printf("\n\n\n\n");

	out = malloc(sizeof(struct Value));

	clrfToValue("-Error checking something\r\n", out);

	printf("%d - value type - %s - err", out->type, out->as.err);

	freeValue(out);

	printf("\n\n\n\n");

	out = malloc(sizeof(struct Value));

	clrfToValue("+OK\r\n", out);

	printf("%d - value type - %s - simple string", out->type, out->as.ss);

	freeValue(out);

	printf("\n\n\n\n");

	out = malloc(sizeof(struct Value));

	clrfToValue("%3\r\n+first\r\n:1\r\n+second\r\n:2\r\n+third\r\n:89\r\n", out);

	printf("%d - value type - object/map \n", out->type);


	for (int i = 0; i < out->as.obj->count; i++)
	{
		printf("%s => %d\n\n", out->as.obj->keys[i]->as.ss, out->as.obj->values[i]->as.i);
	}

	freeValue(out);

	printf("\n\n\n\n");

	out = malloc(sizeof(struct Value));

	clrfToValue("*4\r\n:1\r\n:2\r\n:3\r\n:4\r\n", out);

	printf("%d - value type - array \n", out->type);


	for (int i = 0; i < out->as.arr->count; i++)
	{
		printf("%d\n\n", out->as.arr->items[i]->as.i);
	}

	freeValue(out);


	printf("\n\n\n\n\n");
	out = makePrimitiveValue(VAL_STRING, (void*)"HELLOSZIAAAA");

	char* result = valueToClrf(out);

	printf("Value of conversion to clrf is: %s", result);
	// // Disable output buffering
	// setbuf(stdout, NULL);
	// setbuf(stderr, NULL);

	// // You can use print statements as follows for debugging, they'll be visible when running tests.
	// printf("Logs from your program will appear here!\n");

	// int server_fd, client_addr_len;

	// server_fd = socket(AF_INET, SOCK_STREAM, 0);
	// if (server_fd == -1)
	// {
	// 	printf("Socket creation failed: %s...\n", strerror(errno));
	// 	return 1;
	// }

	// // Since the tester restarts your program quite often, setting SO_REUSEADDR
	// // ensures that we don't run into 'Address already in use' errors
	// int reuse = 1;
	// if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	// {
	// 	printf("SO_REUSEADDR failed: %s \n", strerror(errno));
	// 	return 1;
	// }

	// struct sockaddr_in serv_addr = {
	// 	.sin_family = AF_INET,
	// 	.sin_port = htons(6379),
	// 	.sin_addr = {htonl(INADDR_ANY)},
	// };

	// if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	// {
	// 	printf("Bind failed: %s \n", strerror(errno));
	// 	return 1;
	// }

	// if (listen(server_fd, MAX_CONNECTIONS) != 0)
	// {
	// 	printf("Listen failed: %s \n", strerror(errno));
	// 	return 1;
	// }

	// printf("Waiting for a client to connect...\n");

	// // event loop ->
	// while (1)
	// {
	// 	int client_fd;
	// 	struct sockaddr_in client_addr;
	// 	client_addr_len = sizeof(client_addr);

	// 	client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

	// 	if (client_fd == -1)
	// 	{
	// 		printf("Connection could not be accepted, %s \n", strerror(errno));
	// 		continue;
	// 	}

	// 	int pid = fork();

	// 	if (pid < 0)
	// 	{
	// 		printf("Fork failed on client_fd %d", client_fd);
	// 		close(client_fd);
	// 		continue;
	// 	}

	// 	if(pid > 0) {
	// 		continue;
	// 	}

	// 	char req_buffer[REQ_BUFFER_SIZE];

	// 	while (recv(client_fd, req_buffer, sizeof(req_buffer), 0) != 0)
	// 	{
	// 		if (send(client_fd, PONG_RESPONSE, strlen(PONG_RESPONSE), 0) == -1)
	// 		{
	// 			printf("Response could not be sent, %d -- %s \n", client_fd, strerror(errno));
	// 			break;
	// 		}
	// 	}

	// 	close(client_fd);

	// 	if (pid == 0)
	// 	{
	// 		exit(0);
	// 	}
	// }

	// close(server_fd);

	return 0;
}
