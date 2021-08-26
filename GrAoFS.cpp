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

int SECTOR_SIZE = 512;
int SECTOR_PER_CLUSTER = 4;

// Estrutura de uma entrada
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

int totalFreeSectors() { // retorna a quantidade de setores livres na fat
  unsigned short int value;
  int freeSectors = 0;
  fseek(f, 0, SEEK_SET);
  for (int i = 0; i < 65536; i++) {
    fread( & value, sizeof(unsigned short int), 1, f);
    if (value == 0) {
      freeSectors++;
    }
  }
  return freeSectors;
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

  nSectors = (nSectors * (SECTOR_SIZE * SECTOR_PER_CLUSTER)) / 2;

  for (int i = 0; i < nSectors; i++) {
    if (i < 64) {
      fwrite( & F, sizeof(unsigned short), 1, f); // preenche os setores reservados da FAT
    } else if (i == 64) {
      fwrite( & one, sizeof(unsigned short), 1, f); // preenche a entrada do root directory na FAT
    } else {
      fwrite( & zero, sizeof(unsigned short), 1, f); // limpa qualquer entrada existente na tabela
    }
  }
  cout << "| Formatacao concluida!\n" << endl;
}

void listFiles() {
  int currentSector = 64;
  Entry currentEntry;
  int entries = 0;
  currentEntry.entryType = 1;
  do {
    fseek(f, currentSector * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
    while (currentEntry.entryType != 0 && entries < 64) {

      fread( & currentEntry, sizeof(Entry), 1, f);
      if (currentEntry.entryType == 1) {
        cout << "| Arquivo" << endl;
        cout << "|\t Nome: " << currentEntry.nome << endl;
        cout << "|\t Tamanho: " << currentEntry.tamanho << " (Bytes)" << endl;
      } else {

        if (currentEntry.entryType == 2) {
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
  Entry entry;
  int dirSector = 64;
  char buffer[2048];
  unsigned short int one = 1;
  char filePath[1000];

  cout << "| Digite o caminho completo do arquivo a ser copiado para o GR/AO:" << endl;
  cout << "| ";
  fscanf(stdin, "%s", filePath);

  FILE * arq;
  arq = fopen(filePath, "rb+"); // tenta abrir o arquivo
  if (arq == NULL) {
    cout << "! ERRO - Caminho invalido" << endl;
    menu();
  }

  entry.entryType = 1; // 0x01 - marcador de arquivo
  fseek(arq, 0, SEEK_END); // corre para a ultima posicao
  entry.tamanho = getFileSize(filePath); // retorna o tamanho do arquivo
  char bar = '\\';
  char * ret;
  ret = strrchr(filePath, bar); // procura a ultima ocorrencia do caracter barra
  ret++; // avanca a ultima barra encontrada
  strncpy(entry.nome, ret, 25);
  entry.cluster_inicial = searchFreeSector(); // procura um setor livre

  int nSectors;
  nSectors = ceil((double)(double) entry.tamanho / (SECTOR_SIZE * SECTOR_PER_CLUSTER));
  if (nSectors > totalFreeSectors()) {
    cout << "! Espaco insuficiente" << endl;
    fclose(arq);
    return;
  }

  Entry temp;
  int nEntries = 0;

  fseek(arq, 0, SEEK_SET);
  fseek(f, 64 * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);

  while (fread( & temp, sizeof(Entry), 1, f)) {
    if ((temp.entryType == 1 || temp.entryType == 2) && nEntries < 64) {

      if (!strncmp(temp.nome, entry.nome, 25)) {
        cout << "! Arquivo existente no sistema" << endl;
        return;
      }
      nEntries++;
    } else {

      if (nEntries != 64) {
        fseek(f, dirSector * (SECTOR_SIZE * SECTOR_PER_CLUSTER) + nEntries * sizeof(Entry), SEEK_SET);
        fwrite( & entry, sizeof(Entry), 1, f);
        break;
      } else {
        cout << "| Setor cheio, alocando novo setor..." << endl;
        setOnFAT(searchFreeSector(), dirSector);
        dirSector = searchInFAT(dirSector);
        setOnFAT(one, dirSector);
        fseek(f, dirSector * 64, SEEK_SET);
        fwrite( & entry, sizeof(Entry), 1, f);
        nEntries = 0;
      }
    }
  }
  fflush(f);

  int currentSector = entry.cluster_inicial;
  int newSector = 0;

  fseek(arq, 0, SEEK_SET);
  int size = entry.tamanho % (SECTOR_SIZE * SECTOR_PER_CLUSTER);

  for (nSectors; nSectors >= 1; nSectors--) {
    if (nSectors == 1) {
      char tempBuffer[size];
      fread( & tempBuffer, size, 1, arq);
      fseek(f, (currentSector) * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
      fwrite( & tempBuffer, size, 1, f);
      setOnFAT(one, currentSector);
      fclose(f);

    } else {
      fread( & buffer, (SECTOR_SIZE * SECTOR_PER_CLUSTER), 1, arq);
      fseek(f, (currentSector) * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
      fflush(f);
      fwrite( & buffer, (SECTOR_SIZE * SECTOR_PER_CLUSTER), 1, f);
      setOnFAT(currentSector, currentSector);
      newSector = searchFreeSector();
      setOnFAT(newSector, currentSector);

      newSector = searchFreeSector();
      currentSector = newSector;

    }

  }
  cout << "| Arquivo copiado com sucesso" << endl;

  fclose(arq);
}

void fileToHD() {
  FILE * dest;
  char origin[1000], destiny[1000], buffer[2048];
  unsigned short int currentSector = 64;
  Entry currentEntry;

  cout << "| Digite o nome do arquivo a ser copiado:" << endl;
  cout << "| ";
  cin >> origin;
  cout << "| Digite o caminho destino:" << endl;
  cout << "| ";
  cin >> destiny;

  strcat(destiny, "\\");
  strcat(destiny, origin);
  dest = fopen(destiny, "wb+");
  bool finish = false;

  while (searchInFAT(currentSector) != 1) {
    fseek(f, currentSector * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
    for (int i = 0; i < 64; i++) {
      fread( & currentSector, sizeof(Entry), 1, f);
      if (!strncmp(origin, currentEntry.nome, 25)) {
        currentSector = currentEntry.cluster_inicial;
        while (!finish) {
          if (searchInFAT(currentSector) == 1) {
            finish = true;
          }
          fseek(f, currentSector * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
          fread( & buffer, (SECTOR_SIZE * SECTOR_PER_CLUSTER), 1, f);
          fwrite( & buffer, (SECTOR_SIZE * SECTOR_PER_CLUSTER), 1, dest);
          currentSector = searchInFAT(currentSector);
        }
        fclose(dest);
        cout << "| Arquivo exportado com sucesso" << endl;
        return;
      }
    }
  }
  if (searchInFAT(currentSector == 1)) {
    for (int i = 0; i < 64; i++) {
      fseek(f, (currentSector * (SECTOR_SIZE * SECTOR_PER_CLUSTER)) + (i * sizeof(Entry)), SEEK_SET);
      fread( & currentEntry, sizeof(Entry), 1, f);
      if (currentEntry.entryType != 1 && currentEntry.entryType != 2) {
        fclose(dest);
        cout << "! Falha na exportacao do arquivo"<< endl;
        return;
      }
      if (!strncmp(origin, currentEntry.nome, 25)) {
        currentSector = currentEntry.cluster_inicial;
        int a = currentEntry.tamanho;
        int b = 2048;
        int res;
        res = a % b;
        char tempBuffer[res];
        int nSetores = ceil((double)(double) currentEntry.tamanho / (SECTOR_SIZE * SECTOR_PER_CLUSTER));;

        for (nSetores; nSetores > 0; nSetores--) {
          if (nSetores == 1) {
            fseek(f, currentSector * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
            fread( & tempBuffer, res, 1, f);
            fwrite( & tempBuffer, res, 1, dest);
            currentSector = searchInFAT(currentSector);
          } else {
            fseek(f, currentSector * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
            fread( & buffer, (SECTOR_SIZE * SECTOR_PER_CLUSTER), 1, f);
            fwrite( & buffer, (SECTOR_SIZE * SECTOR_PER_CLUSTER), 1, dest);
            currentSector = searchInFAT(currentSector);
          }
        }
        fclose(dest);
        cout << "| Arquivo exportado com sucesso" << endl;
        return;
      }

    }
  }
}

void mkDir() {
  unsigned short int one = 1;
  char dirName[25];
  Entry newEntry;
  Entry auxEntry;

  int currentEntry = 64;

  cout << "| Digite o nome do diretorio: ";
  scanf("%s", dirName);

  newEntry.entryType = 2; // 0x02 - marcador diretorio
  strncpy(newEntry.nome, dirName, 25);
  newEntry.cluster_inicial = searchFreeSector();
  setOnFAT(one, newEntry.cluster_inicial);
  newEntry.tamanho = 0;

  while (searchInFAT(currentEntry) != 1) {
    currentEntry = searchInFAT(currentEntry);
    fseek(f, currentEntry * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
    for (int i = 0; i < 64; i++) {
      fread( & auxEntry, sizeof(Entry), 1, f);
      if (!strncmp(auxEntry.nome, newEntry.nome, 25)) {
        cout << "! Diretorio ja existente" << endl;
        return;
      }
    }
  }

  fseek(f, currentEntry * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);

  for (int i = 0; i <= 64; i++) {
    if (i == 64) {
      setOnFAT(searchFreeSector(), currentEntry);
      fseek(f, searchInFAT(currentEntry) * sizeof(unsigned short int), SEEK_SET);
      unsigned short int one = 1;
      fwrite( & one, sizeof(unsigned short int), 1, f);
      fseek(f, searchInFAT(currentEntry) * (SECTOR_SIZE * SECTOR_PER_CLUSTER), SEEK_SET);
      fwrite( & newEntry, sizeof(Entry), 1, f);
    }
    fseek(f, currentEntry * (SECTOR_SIZE * SECTOR_PER_CLUSTER) + i * sizeof(Entry), SEEK_SET);
    fread( & auxEntry, sizeof(Entry), 1, f);
    fflush(f);

    if (!strncmp(auxEntry.nome, newEntry.nome, 25)) {
      cout << "! Diretorio ja existente" << endl;
      return;
    }
    if (auxEntry.entryType != 2 && auxEntry.entryType != 1) {
      fseek(f, currentEntry * (SECTOR_SIZE * SECTOR_PER_CLUSTER) + i * sizeof(Entry), SEEK_SET);
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
    cout << "| 2 - Listar arquivos e diretorios             |" << endl;
    cout << "| 3 - Criar sub-diretorio                      |" << endl;
    cout << "| 4 - Copiar arquivo para o sistema de arquivo |" << endl;
    cout << "| 5 - Copiar arquivo para o disco              |" << endl;
    cout << "| 0 - Sair                                     |"<< endl;
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
      listFiles();
      fclose(f);
      break;
    case 3:
      openImage();
      mkDir();
      fclose(f);
      break;
    case 4:
      openImage();
      fileToFS();
      fclose(f);
      break;
    case 5:
      openImage();
      fileToHD();
      fclose(f);
      break;
    case 0:
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