#include <cstdio>
#include <string.h>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <math.h>
#include <fcntl.h>
using namespace std;

FILE * f;

//Estrutura de uma entrada
typedef struct {
  unsigned char entryType; // 0xAA para arquivo e 0xFF para sub-diretorio
  char nome[25];
  unsigned short cluster_inicial;
  unsigned int tamanho;
}
__attribute__((packed)) entry;

void abreImagem() {
  f = fopen("./images/imagem.img", "rb+");
  if (f == NULL) {
    cout << "Erro" << endl;
    exit(0);
  }
}

int procuraSetorLivre() {
  unsigned short int entry;
  fseek(f, 0, SEEK_SET);
  for (int i = 0; i < 65536; i++) {
    fread( & entry, 2, 1, f);
    if (entry == 0)
      return i;
  }
  return -1;
}

void setaValorNaFat(unsigned short int value, int pos) {
  fseek(f, pos * 2, SEEK_SET);
  fwrite( & value, sizeof(unsigned short int), 1, f);

}

unsigned short int buscaValorNaFat(int pos) {
  unsigned short int aux;
  fseek(f, pos * 2, SEEK_SET);
  fread( & aux, sizeof(unsigned short int), 1, f);
  return aux;
}
int totalSetoresLivres() { //retorna a quantidade de setores livres na fat
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

void formatar() {
  unsigned short zero = 0;
  unsigned short um = 1;
  unsigned short dois = 2;
  int tamanho = 0;
  int i;
  cout << "Digite a quantidade de setores a serem formatados" << endl;
  cin >> tamanho;
  tamanho = (tamanho * 2048) / 2; // dividido por 2

  for (i = 0; i < tamanho; i++) {
    if (i < 64) {
      fwrite( & dois, sizeof(unsigned short), 1, f);
      //preenche os setores reservados da FAT
    } else if (i == 64) {
      fwrite( & um, sizeof(unsigned short), 1, f);
      // preenche a entrada do root directory na FAT
    } else {
      fwrite( & zero, sizeof(unsigned short), 1, f);
      //limpa qualquer entrada existente na tabela
    }
  }
  cout << "formatacao concluida!\n\n" << endl;
}

void listaarquivos() {
  int setorAtual = 64;
  entry entradaAtual;
  int entradas = 0;
  entradaAtual.entryType = 170; // AA
  do {
    fseek(f, setorAtual * 2048, SEEK_SET);
    while (entradaAtual.entryType != 0 && entradas < 64) {

      fread( & entradaAtual, sizeof(entry), 1, f);
      if (entradaAtual.entryType == 170) {
        printf("tipo: Arquivo\n nome: %s\n Tamanho: %d (Bytes)\n\n", entradaAtual.nome, entradaAtual.tamanho);
      } else {

        if (entradaAtual.entryType == 255) {
          printf("tipo: Diretorio\n nome: %s\n\n", entradaAtual.nome);
        }
      }
      if (entradaAtual.entryType == 0) {
        entradas++;
        break;
      }
      entradas++;
    }
    if (buscaValorNaFat(setorAtual) != 1) {
      setorAtual = buscaValorNaFat(setorAtual);
      entradas = 0;
      continue;
    } else {
      break;
    }
  } while (buscaValorNaFat(setorAtual) != 1);
}

void arquivoParaPendrive() {
  FILE * arquivo;
  entry entrada;
  int dirSector = 64;
  char buffer[2048];
  unsigned short int um = 1;
  char caminhoArquivo[1000];

  cout << "Digite o caminho do arquivo a ser copiado para o pendrive\n" << endl;
  fscanf(stdin, "%s", caminhoArquivo);

  FILE * arq;
  arq = fopen(caminhoArquivo, "rb+"); // tenta abrir o arquivo

  if (arq == NULL) {
    printf("Erro, diretorio invalido\n");
    exit(0);
  }

  //c:\\users\\mateus\\documents\\arquivoTeste.txt

  entrada.entryType = 170; //define o tipo da entrada (0xAA = arquivo)
  fseek(arq, 0, SEEK_END); //corre para a ultima posi��o
  entrada.tamanho = getFileSize(caminhoArquivo); //retorna o tamanho do arquivo
  char barra = '/';
  char * ret;
  ret = strrchr(caminhoArquivo, barra); //procura a ultima ocorrencia do caracter barra
  ret++; //avanca a ultima barra encontrada
  strncpy(entrada.nome, ret, 25);
  entrada.cluster_inicial = procuraSetorLivre(); //procura um setor livre

  int nSetores;
  nSetores = ceil((double)(double) entrada.tamanho / 2048);
  cout << "numero de setores: " << nSetores << endl;
  if (nSetores > totalSetoresLivres()) {
    cout << "Espaco em disco insuficiente, arquivo muito grande" << endl;
    fclose(arq);
    return;
  }

  entry dump;
  int nEntradas = 0;

  fseek(arq, 0, SEEK_SET);
  fseek(f, 64 * 2048, SEEK_SET);

  while (fread( & dump, sizeof(entry), 1, f)) {
    if ((dump.entryType == 170 || dump.entryType == 255) && nEntradas < 64) {

      if (!strncmp(dump.nome, entrada.nome, 25)) {
        cout << "Arquivo ja existente no disco" << endl;
        return;
      }
      nEntradas++;
    } else {

      if (nEntradas != 64) {
        cout << dirSector * 2048 + nEntradas << endl;
        cout << entrada.nome << endl;
        fseek(f, dirSector * 2048 + nEntradas * sizeof(entry), SEEK_SET);
        fwrite( & entrada, sizeof(entry), 1, f);
        break;
      } else {
        cout << "Setor cheio, alocando novo setor" << endl;
        setaValorNaFat(procuraSetorLivre(), dirSector);
        dirSector = buscaValorNaFat(dirSector);
        setaValorNaFat(um, dirSector);
        fseek(f, dirSector * 64, SEEK_SET);
        fwrite( & entrada, sizeof(entry), 1, f);
        nEntradas = 0;
      }
    }
  }
  fflush(f);

  int setorAtual = entrada.cluster_inicial;
  int novoSetor = 0;

  fseek(arq, 0, SEEK_SET);
  int tamanho = entrada.tamanho % 2048;

  for (nSetores; nSetores >= 1; nSetores--) {
    if (nSetores == 1) {
      char buffer2[tamanho];
      fread( & buffer2, tamanho, 1, arq);
      fseek(f, (setorAtual) * 2048, SEEK_SET);
      fwrite( & buffer2, tamanho, 1, f);
      setaValorNaFat(um, setorAtual);
      fclose(f);

    } else {
      fread( & buffer, 2048, 1, arq);
      fseek(f, (setorAtual) * 2048, SEEK_SET);
      fflush(f);
      fwrite( & buffer, 2048, 1, f);
      setaValorNaFat(setorAtual, setorAtual);
      novoSetor = procuraSetorLivre();
      setaValorNaFat(novoSetor, setorAtual);

      novoSetor = procuraSetorLivre();
      setorAtual = novoSetor;

    }

  }
  cout << "Arquivo copiado com sucesso" << endl;

  fclose(arq);
}

void arquivoParaHD() {
  FILE * dest;
  char origem[1000], destino[1000], buffer[2048];
  unsigned short int setorAtual = 64;
  entry entradaAtual;

  cout << "Digite o nome do arquivo a ser copiado" << endl;
  cin >> origem;
  cout << "Digite o caminho de exportacao do arquivo" << endl;
  cin >> destino;

  strcat(destino, "\\");
  strcat(destino, origem);
  cout << destino << endl;
  dest = fopen(destino, "wb+");
  int terminou = 0;

  while (buscaValorNaFat(setorAtual) != 1) {
    fseek(f, setorAtual * 2048, SEEK_SET);
    for (int i = 0; i < 64; i++) {
      fread( & setorAtual, sizeof(entry), 1, f);
      if (!strncmp(origem, entradaAtual.nome, 25)) {
        setorAtual = entradaAtual.cluster_inicial;
        while (!terminou) {
          if (buscaValorNaFat(setorAtual) == 1) {
            terminou = 1;
          }
          fseek(f, setorAtual * 2048, SEEK_SET);
          fread( & buffer, 2048, 1, f);
          fwrite( & buffer, 2048, 1, dest);
          setorAtual = buscaValorNaFat(setorAtual);
        }
        fclose(dest);
        cout << "Arquivo exportado com sucesso" << endl;
        return;
      }
    }
  }
  if (buscaValorNaFat(setorAtual == 1)) {
    for (int i = 0; i < 64; i++) {
      fseek(f, (setorAtual * 2048) + (i * sizeof(entry)), SEEK_SET);
      fread( & entradaAtual, sizeof(entry), 1, f);
      if (entradaAtual.entryType != 170 && entradaAtual.entryType != 255) {
        fclose(dest);
        cout << "\n\nFalha na exportacao do arquivo\n"<< endl;
        return;
      }
      if (!strncmp(origem, entradaAtual.nome, 25)) {
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
            setorAtual = buscaValorNaFat(setorAtual);
          } else {
            fseek(f, setorAtual * 2048, SEEK_SET);
            fread( & buffer, 2048, 1, f);
            fwrite( & buffer, 2048, 1, dest);
            setorAtual = buscaValorNaFat(setorAtual);
          }
        }
        fclose(dest);
        printf("\nArquivo exportado com sucesso\n");
        return;
      }

    }
  }
}

//C:\Users\\Mateus\\Documents\\trabSo3\\antigo\\

void criardiretorio() {
  unsigned short int um = 1;
  char nomeDir[25];
  entry newEntry;
  entry auxEntry;

  int currentEntry = 64;

  printf("Digite o nome do diretorio: ");
  scanf("%s", nomeDir);

  newEntry.entryType = 255;
  strncpy(newEntry.nome, nomeDir, 25);
  //cout<<newEntry.nome<< endl;
  //
  newEntry.cluster_inicial = procuraSetorLivre();
  setaValorNaFat(um, newEntry.cluster_inicial);
  newEntry.tamanho = 0;

  while (buscaValorNaFat(currentEntry) != 1) {
    currentEntry = buscaValorNaFat(currentEntry);
    fseek(f, currentEntry * 2048, SEEK_SET);
    for (int i = 0; i < 64; i++) {
      fread( & auxEntry, sizeof(entry), 1, f);
      //printf("aux: %s\t neW: %s\n", auxEntry.nome, newEntry.nome);
      if (!strncmp(auxEntry.nome, newEntry.nome, 25)) {
        printf("diretorio ja existente\n");
        return;
      }
    }
  }

  fseek(f, currentEntry * 2048, SEEK_SET);

  for (int i = 0; i <= 64; i++) {
    if (i == 64) {
      setaValorNaFat(procuraSetorLivre(), currentEntry);
      fseek(f, buscaValorNaFat(currentEntry) * sizeof(unsigned short int), SEEK_SET);
      unsigned short int um = 1;
      fwrite( & um, sizeof(unsigned short int), 1, f);
      fseek(f, buscaValorNaFat(currentEntry) * 2048, SEEK_SET);
      fwrite( & newEntry, sizeof(entry), 1, f);
    }
    fseek(f, currentEntry * 2048 + i * sizeof(entry), SEEK_SET); //seguro morreu de velho
    fread( & auxEntry, sizeof(entry), 1, f);
    fflush(f);

    if (!strncmp(auxEntry.nome, newEntry.nome, 25)) {
      printf("diretorio ja existente\n");
      return;
    }
    if (auxEntry.entryType != 255 && auxEntry.entryType != 170) {
      fseek(f, currentEntry * 2048 + i * sizeof(entry), SEEK_SET);
      fwrite( & newEntry, sizeof(entry), 1, f);
      break;
    }

  }

}

void menu() {
  while (1) {
    int op;
    cout << "\n\t--------menu--------\n" << endl;
    cout << "\t1 - formatar disco\n" << endl;
    cout << "\t2 - copiar arquivo para o pendrive\n" << endl;
    cout << "\t3 - copiar arquivo para o disco\n" << endl;
    cout << "\t4 - listar arquivos do diretorio atual\n" << endl;
    cout << "\t5 - criar sub-diretorio\n" << endl;
    cout << "\t6 - sair\n" << endl;
    cout << "opcao desejada: " << endl;
    scanf("%d", & op);
    cout << "\n" << endl;

    switch (op) {
    case 1:
      abreImagem();
      formatar();
      fclose(f);
      break;
    case 2:
      abreImagem();
      arquivoParaPendrive();
      fclose(f);
      break;
    case 3:
      abreImagem();
      arquivoParaHD();
      fclose(f);
      break;
    case 4:
      abreImagem();
      listaarquivos();
      fclose(f);
      break;
    case 5:
      abreImagem();
      criardiretorio();
      fclose(f);
      break;
    case 6:
      exit(0);
      break;
    default:
      exit(0);
      break;
    }
  }
}

int main() {
  cout << "GR/AO OS" << endl;
  menu();
  return 0;
}