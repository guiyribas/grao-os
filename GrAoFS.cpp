#include <cstdio>
#include <string.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <math.h>
#include <io.h>
#include <fcntl.h>
using namespace std;

void menu();

FILE * f;

//Estrutura de uma entrada
typedef struct {
  unsigned char entryType; // 0xAA para arquivo e 0xFF para sub-diretorio
  char nome[25];
  unsigned short cluster_inicial;
  unsigned int tamanho;
}
__attribute__((packed)) Entry;

void openImage() {
  f = fopen("imagem.img", "rb+");
  if (f == NULL) {
    cout << "Erro" << endl;
    exit(0);
  }
}

int searchFreeSector() {
  unsigned short int entry;
  fseek(f, 0, SEEK_SET);
  for (int i = 0; i < 65536; i++) {
    fread( & entry, 2, 1, f);
    if (entry == 0)
      return i;
  }
  return -1;
}

void setOnFAT(unsigned short int value, int pos) {
  fseek(f, pos * 2, SEEK_SET);
  fwrite( & value, sizeof(unsigned short int), 1, f);

}

unsigned short int searchInFAT(int pos) {
  unsigned short int aux;
  fseek(f, pos * 2, SEEK_SET);
  fread( & aux, sizeof(unsigned short int), 1, f);
  return aux;
}

int totalFreeSectors() { //retorna a quantidade de setores livres na fat
  unsigned short int value;
  int setoresLivres = 0;
  fseek(f, 0, SEEK_SET);
  for (int i = 0; i < 65536; i++) {
    fread( & value, sizeof(unsigned short int), 1, f);
    if (value == 0) {
      setoresLivres++;
    }
  }
  return setoresLivres;
}

int getFileSize(const std::string & fileName) // retorna o tamanho do arquivo
{
  ifstream file(fileName.c_str(), ifstream:: in | ifstream::binary | ifstream::ate);

  if (!file.is_open()) {
    return -1;
  }

  int fileSize = file.tellg();
  file.close();

  return fileSize;
}
//       C:\\Users\\Mateus\\Documents\\arquivoTeste.txt

void format() {
  unsigned short zero = 0; // 0x00 for free sectors
  unsigned short one = 1; // 0x01 for root directory sector
  unsigned short F = 15; // 0x0F for reserved FAT sectors 
  int nSectors = 0;
  cout << "| Digite a quantidade de setores a serem formatados (0 a 65536): ";
  cin >> nSectors;

  if (nSectors < 0 || nSectors > 65536) {
    cout << "! Numero de setores invalido... \n" << endl;
    menu();
  }

  nSectors = (nSectors * 2048) / 2;

  for (int i = 0; i < nSectors; i++) {
    if (i < 64) {
      fwrite( & F, sizeof(unsigned short), 1, f); //preenche os setores reservados da FAT
    } else if (i == 64) {
      fwrite( & one, sizeof(unsigned short), 1, f); // preenche a entrada do root directory na FAT
    } else {
      fwrite( & zero, sizeof(unsigned short), 1, f); //limpa qualquer entrada existente na tabela
    }
  }
  cout << "| Formatacao concluida!\n" << endl;
}

void listFiles() {
  int currentSector = 64;
  Entry currentEntry;
  int entries = 0;
  currentEntry.entryType = 170;
  do {
    fseek(f, currentSector * 2048, SEEK_SET);
    while (currentEntry.entryType != 0 && entries < 64) {

      fread( & currentEntry, sizeof(Entry), 1, f);
      if (currentEntry.entryType == 170) {
        cout << "| Arquivo" << endl;
        cout << "|\t Nome: " << currentEntry.nome << endl;
        cout << "|\t Tamanho: " << currentEntry.tamanho << " (Bytes)" << endl;
      } else {

        if (currentEntry.entryType == 255) {
          cout << "| Diretorio" << endl;
          cout << "|\t Nome: " << currentEntry.nome << endl;
        }
      }
      if (currentEntry.entryType == 0) {
        entries++;
        break;
      }
      entries++;
    }
    if (searchInFAT(currentSector) != 1) {
      currentSector = searchInFAT(currentSector);
      entries = 0;
      continue;
    } else {
      break;
    }
  } while (searchInFAT(currentSector) != 1);
}

void fileToFS() {
  FILE * arquivo;
  Entry entry;
  int dirSector = 64;
  char buffer[2048];
  unsigned short int um = 1;
  char caminhoArquivo[1000];

  cout << "| Digite o caminho do arquivo a ser copiado para o sistema de arquivo" << endl;
  fscanf(stdin, "%s", caminhoArquivo);

  FILE * arq;
  arq = fopen(caminhoArquivo, "rb+"); // tenta abrir o arquivo
  if (arq == NULL) {
    cout << "! ERRO - Caminho invalido" << endl;
    exit(0);
  }

  //c:\\users\\mateus\\documents\\arquivoTeste.txt

  entry.entryType = 170; //define o tipo da entrada (0xAA = arquivo)
  fseek(arq, 0, SEEK_END); //corre para a ultima posi��o
  entry.tamanho = getFileSize(caminhoArquivo); //retorna o tamanho do arquivo
  char barra = '\\';
  char * ret;
  ret = strrchr(caminhoArquivo, barra); //procura a ultima ocorrencia do caracter barra
  ret++; //avanca a ultima barra encontrada
  strncpy(entry.nome, ret, 25);
  entry.cluster_inicial = searchFreeSector(); //procura um setor livre

  int nSetores;
  nSetores = ceil((double)(double) entry.tamanho / 2048);
  cout << "| Numero de setores: " << nSetores << endl;
  if (nSetores > totalFreeSectors()) {
    cout << "! Espaco insuficiente" << endl;
    fclose(arq);
    return;
  }

  Entry dump;
  int nEntradas = 0;

  fseek(arq, 0, SEEK_SET);
  fseek(f, 64 * 2048, SEEK_SET);

  while (fread( & dump, sizeof(Entry), 1, f)) {
    if ((dump.entryType == 170 || dump.entryType == 255) && nEntradas < 64) {

      if (!strncmp(dump.nome, entry.nome, 25)) {
        cout << "! Arquivo existente no sistema" << endl;
        return;
      }
      nEntradas++;
    } else {

      if (nEntradas != 64) {
        cout << dirSector * 2048 + nEntradas << endl;
        cout << entry.nome << endl;
        fseek(f, dirSector * 2048 + nEntradas * sizeof(Entry), SEEK_SET);
        fwrite( & entry, sizeof(Entry), 1, f);
        break;
      } else {
        cout << "| Setor cheio, alocando novo setor..." << endl;
        setOnFAT(searchFreeSector(), dirSector);
        dirSector = searchInFAT(dirSector);
        setOnFAT(um, dirSector);
        fseek(f, dirSector * 64, SEEK_SET);
        fwrite( & entry, sizeof(Entry), 1, f);
        nEntradas = 0;
      }
    }
  }
  fflush(f);

  int setorAtual = entry.cluster_inicial;
  int novoSetor = 0;

  fseek(arq, 0, SEEK_SET);
  int tamanho = entry.tamanho % 2048;

  for (nSetores; nSetores >= 1; nSetores--) {
    if (nSetores == 1) {
      char buffer2[tamanho];
      fread( & buffer2, tamanho, 1, arq);
      fseek(f, (setorAtual) * 2048, SEEK_SET);
      fwrite( & buffer2, tamanho, 1, f);
      setOnFAT(um, setorAtual);
      fclose(f);

    } else {
      fread( & buffer, 2048, 1, arq);
      fseek(f, (setorAtual) * 2048, SEEK_SET);
      fflush(f);
      fwrite( & buffer, 2048, 1, f);
      setOnFAT(setorAtual, setorAtual);
      novoSetor = searchFreeSector();
      setOnFAT(novoSetor, setorAtual);

      novoSetor = searchFreeSector();
      setorAtual = novoSetor;

    }

  }
  cout << "| Arquivo copiado com sucesso" << endl;

  fclose(arq);
}

void fileToHD() {
  FILE * dest;
  char origin[1000], destiny[1000], buffer[2048];
  unsigned short int setorAtual = 64;
  Entry entradaAtual;

  cout << "| Digite o nome do arquivo a ser copiado" << endl;
  cin >> origin;
  cout << "| Digite o caminho destino" << endl;
  cin >> destiny;

  strcat(destiny, "\\");
  strcat(destiny, origin);
  cout << destiny << endl;
  dest = fopen(destiny, "wb+");
  int terminou = 0;

  while (searchInFAT(setorAtual) != 1) {
    fseek(f, setorAtual * 2048, SEEK_SET);
    for (int i = 0; i < 64; i++) {
      fread( & setorAtual, sizeof(Entry), 1, f);
      if (!strncmp(origin, entradaAtual.nome, 25)) {
        setorAtual = entradaAtual.cluster_inicial;
        while (!terminou) {
          if (searchInFAT(setorAtual) == 1) {
            terminou = 1;
          }
          fseek(f, setorAtual * 2048, SEEK_SET);
          fread( & buffer, 2048, 1, f);
          fwrite( & buffer, 2048, 1, dest);
          setorAtual = searchInFAT(setorAtual);
        }
        fclose(dest);
        cout << "| Arquivo exportado com sucesso" << endl;
        return;
      }
    }
  }
  if (searchInFAT(setorAtual == 1)) {
    for (int i = 0; i < 64; i++) {
      fseek(f, (setorAtual * 2048) + (i * sizeof(Entry)), SEEK_SET);
      fread( & entradaAtual, sizeof(Entry), 1, f);
      if (entradaAtual.entryType != 170 && entradaAtual.entryType != 255) {
        fclose(dest);
        cout << "! Falha na exportacao do arquivo"<< endl;
        return;
      }
      if (!strncmp(origin, entradaAtual.nome, 25)) {
        setorAtual = entradaAtual.cluster_inicial;
        int a = entradaAtual.tamanho, b = 2048, res;
        res = a % b;
        char buffer2[res];
        int nSetores = ceil((double)(double) entradaAtual.tamanho / 2048);;

        for (nSetores; nSetores > 0; nSetores--) {
          if (nSetores == 1) {
            fseek(f, setorAtual * 2048, SEEK_SET);
            fread( & buffer2, res, 1, f);
            fwrite( & buffer2, res, 1, dest);
            setorAtual = searchInFAT(setorAtual);
          } else {
            fseek(f, setorAtual * 2048, SEEK_SET);
            fread( & buffer, 2048, 1, f);
            fwrite( & buffer, 2048, 1, dest);
            setorAtual = searchInFAT(setorAtual);
          }
        }
        fclose(dest);
        cout << "| Arquivo exportado com sucesso" << endl;
        return;
      }

    }
  }
}

//C:\Users\\Mateus\\Documents\\trabSo3\\antigo\\

void mkDir() {
  unsigned short int um = 1;
  char nomeDir[25];
  Entry newEntry;
  Entry auxEntry;

  int currentEntry = 64;

  cout << "| Digite o nome do diretorio: ";
  scanf("%s", nomeDir);

  newEntry.entryType = 255;
  strncpy(newEntry.nome, nomeDir, 25);
  //cout<<newEntry.nome<< endl;
  //
  newEntry.cluster_inicial = searchFreeSector();
  setOnFAT(um, newEntry.cluster_inicial);
  newEntry.tamanho = 0;

  while (searchInFAT(currentEntry) != 1) {
    currentEntry = searchInFAT(currentEntry);
    fseek(f, currentEntry * 2048, SEEK_SET);
    for (int i = 0; i < 64; i++) {
      fread( & auxEntry, sizeof(Entry), 1, f);
      //printf("aux: %s\t neW: %s\n", auxEntry.nome, newEntry.nome);
      if (!strncmp(auxEntry.nome, newEntry.nome, 25)) {
        cout << "! Diretorio ja existente" << endl;
        return;
      }
    }
  }

  fseek(f, currentEntry * 2048, SEEK_SET);

  for (int i = 0; i <= 64; i++) {
    if (i == 64) {
      setOnFAT(searchFreeSector(), currentEntry);
      fseek(f, searchInFAT(currentEntry) * sizeof(unsigned short int), SEEK_SET);
      unsigned short int um = 1;
      fwrite( & um, sizeof(unsigned short int), 1, f);
      fseek(f, searchInFAT(currentEntry) * 2048, SEEK_SET);
      fwrite( & newEntry, sizeof(Entry), 1, f);
    }
    fseek(f, currentEntry * 2048 + i * sizeof(Entry), SEEK_SET); //seguro morreu de velho
    fread( & auxEntry, sizeof(Entry), 1, f);
    fflush(f);

    if (!strncmp(auxEntry.nome, newEntry.nome, 25)) {
      cout << "! Diretorio ja existente" << endl;
      return;
    }
    if (auxEntry.entryType != 255 && auxEntry.entryType != 170) {
      fseek(f, currentEntry * 2048 + i * sizeof(Entry), SEEK_SET);
      fwrite( & newEntry, sizeof(Entry), 1, f);
      break;
    }

  }

}

void menu() {
  while (1) {
    unsigned int op;
    cout << "|----------------------------------------------|" << endl;
    cout << "| 1 - Formatar disco                           |" << endl;
    cout << "| 2 - Copiar arquivo para o sitema de arquivo  |" << endl;
    cout << "| 3 - Copiar arquivo para o disco              |" << endl;
    cout << "| 4 - Listar arquivos do diretorio atual       |" << endl;
    cout << "| 5 - Criar sub-diretorio                      |" << endl;
    cout << "| 6 - Sair                                     |"<< endl;
    cout << "|----------------------------------------------|" << endl;
    cout << "| Opcao desejada: ";

    cin >> op;

    switch (op) {
    case 1:
      openImage();
      format();
      fclose(f);
      break;
    case 2:
      openImage();
      fileToFS();
      fclose(f);
      break;
    case 3:
      openImage();
      fileToHD();
      fclose(f);
      break;
    case 4:
      openImage();
      listFiles();
      fclose(f);
      break;
    case 5:
      openImage();
      mkDir();
      fclose(f);
      break;
    case 6:
      cout << "| Saindo...";
      exit(0);
      break;
    default:
      cout << "| Comando invalido" << endl;
      menu();
      break;
    }
    
  }
}

int main() {
  cout << "|----------------------------------------------|" << endl;
  cout << "|------         GR/AO FILE SYSTEM        ------|" << endl;
  cout << "|----------------------------------------------|" << endl;
  cout << "|- Desenvolvido por:                           |" << endl;
  cout << "|- Angelo Orssatto                             |" << endl;
  cout << "|- Guilherme Ribas                             |" << endl;
  menu();
  return 0;
}
