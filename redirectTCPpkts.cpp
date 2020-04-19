#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <winsock.h>
#ifndef INADDR_NONE
	#define INADDR_NONE 0xffffffff
#endif
#define WIN32_LEAN_AND_MEAN
#define bzero(p, l) memset(p, 0, l)
#define bcopy(s, t, l) memmove(t, s, l)
#define MAXCLIENTS 20
#define IDLETIMEOUT 300

// cpp
#include <cstdlib>
#include <iostream>
using namespace std;
//---

// Estrutura p sockets clientes
struct struct_Cliente {
	int inuse;
	SOCKET INsock, OUTsock;
	time_t activity;
};


int main(int argc, char *argv[]) { 

	SOCKET INsock;
	char buf[4096];
	struct sockaddr_in INaddress, OUTaddress;
	int i;
	struct struct_Cliente clients[MAXCLIENTS];

	WSADATA wsadata;
	WSAStartup(MAKEWORD(1,1), &wsadata);

	//Testa o numero de argumentos da linha de comando

	if (argc != 5) {
		std::cout << "\n  Este programa redireciona packotes TCP de um par ip/port para outro par ip/port\n\n";
		std::cout << "    Uso: " << argv[0] << " IP1 PORT1 IP2 PORT2\n";
		return EXIT_FAILURE;
	}

	// Reseta todas estruturas de clientes
	for (i = 0; i < MAXCLIENTS; i++)	clients[i].inuse = 0;

	// Configura o host/porta de entrada
	bzero(&INaddress, sizeof(struct sockaddr_in));
	INaddress.sin_family = AF_INET;	                     // tipo UDP, TCP, etc. 
	INaddress.sin_addr.s_addr = inet_addr(argv[1]);
	INaddress.sin_port = htons((unsigned short) atol(argv[2]));

	if (!INaddress.sin_port) {
		std::cout << "Porta de entrada invalida!" << std::endl;
		return EXIT_FAILURE;
	}

	if (INaddress.sin_addr.s_addr == INADDR_NONE) {
		struct hostent *n;
		if ((n = gethostbyname(argv[1])) == NULL) {
			std::cout << "Host de entrada invalido!" << std::endl;
			return EXIT_FAILURE;
		}    
		bcopy(n->h_addr, (char *) &INaddress.sin_addr, n->h_length);
	}

	// Configura o par host/porta de saída
	bzero(&OUTaddress, sizeof(struct sockaddr_in));
	OUTaddress.sin_family = AF_INET;
	OUTaddress.sin_port = htons((unsigned short) atol(argv[4]));

	if (!OUTaddress.sin_port) {
		std::cout << "Porta de saida invalida!" << std::endl;
		return EXIT_FAILURE;
	}

	OUTaddress.sin_addr.s_addr = inet_addr(argv[3]);

	if (OUTaddress.sin_addr.s_addr == INADDR_NONE) {
		struct hostent *n;
		if ((n = gethostbyname(argv[3])) == NULL) {
			std::cout << "Host de saida invalido!" << std::endl;
			return EXIT_FAILURE;
		}    
		bcopy(n->h_addr, (char *) &OUTaddress.sin_addr, n->h_length);
	}

	// Cria o socket de entrada
	if ((INsock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		std::cout << "Erro na criacao socket de entrada!" << std::endl;
		return EXIT_FAILURE;
	}

	if (bind(INsock, (struct sockaddr *)&INaddress, sizeof(INaddress))) {
		std::cout << "Erro no socket de entrada!" << std::endl;
		return EXIT_FAILURE;
	}

	if (listen(INsock, 5)) {
		std::cout << "Erro na escuta do socket de entrada!" << std::endl;
		return EXIT_FAILURE;
	}

	// Muda a porta na estrutura cliente para 0, e será usada para sockets de saída locais
	INaddress.sin_port = htons(0);

	// Loop principal
	while (TRUE)  {

		fd_set fdsr;
		int maxsock;
		struct timeval tv = {1,0};
		time_t now = time(NULL);

		// Cria lista p checagem de sockets
		FD_ZERO(&fdsr);
		FD_SET(INsock, &fdsr);
		maxsock = (int) INsock;
		for (i = 0; i < MAXCLIENTS; i++) {
			if (clients[i].inuse) {
				FD_SET(clients[i].INsock, &fdsr);
				if ((int) clients[i].INsock > maxsock)   maxsock = (int) clients[i].INsock;
				FD_SET(clients[i].OUTsock, &fdsr);
				if ((int) clients[i].OUTsock > maxsock)  maxsock = (int) clients[i].OUTsock;
			}
		}

		if (select(maxsock + 1, &fdsr, NULL, NULL, &tv) < 0) return EXIT_FAILURE;

		// Verifica novas conexoes
		if (FD_ISSET(INsock, &fdsr)) {
			SOCKET NEWsock = accept(INsock, NULL, 0);
			for (i = 0; i < MAXCLIENTS; i++)
			if (!clients[i].inuse) break;

			if (i < MAXCLIENTS)	{
			// Conecta um novo socket ao socket de saída
				SOCKET OUTsock;
				if ((OUTsock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
				    std::cout << "Erro na criacao do socket de saida!" << std::endl;
				    closesocket(NEWsock);
				} else if (bind(OUTsock, (struct sockaddr *)&INaddress, sizeof(INaddress))) {
			        std::cout << "Erro no socket de saida!" << std::endl;
			        closesocket(NEWsock);
			        closesocket(OUTsock);
				} else if (connect(OUTsock, (struct sockaddr *)&OUTaddress, sizeof(OUTaddress))) {
					std::cout << "Erro na conexao do socket de saida!" << std::endl;
					closesocket(NEWsock);
					closesocket(OUTsock);
				} else {
					clients[i].OUTsock = OUTsock;
					clients[i].INsock = NEWsock;
					clients[i].activity = now;
					clients[i].inuse = 1;
					std::cout << "Cliente redirecionado!" << std::endl;
				}
			} else {
			        std::cout << "Fila de clientes cheia!" << std::endl;
			        closesocket(NEWsock);
			}
		}

		// Trata os dados das conexoes clientes
		for (i = 0; i < MAXCLIENTS; i++) {
			int nbyt, closeclient = 0;
			if (!clients[i].inuse) continue;
			else if (FD_ISSET(clients[i].INsock, &fdsr)) {
				if ((nbyt = recv(clients[i].INsock, buf, sizeof(buf), 0)) <= 0 || send(clients[i].OUTsock, buf, nbyt, 0) <= 0) closeclient = 1;
				else clients[i].activity = now;
			} else if (FD_ISSET(clients[i].OUTsock, &fdsr)) {
				if ((nbyt = recv(clients[i].OUTsock, buf, sizeof(buf), 0)) <= 0 || send(clients[i].INsock, buf, nbyt, 0) <= 0) closeclient = 1;
				else clients[i].activity = now;
			} else if (now - clients[i].activity > IDLETIMEOUT) {
				closeclient = 1;
			}

			if (closeclient) {
				closesocket(clients[i].INsock);
				closesocket(clients[i].OUTsock);
				clients[i].inuse = 0;
			}     
		}

	}

	return 0;

}









