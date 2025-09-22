#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

char *paths[100]; // Va a almacenar las rutas de variables de entorno, luego lo podemos cambiar a un archivo plano

char *eliminarEnter(char *comando)
{
    for (int i = 0; comando[i] != '\0'; i++)
    {
        if (comando[i] == '\n')
        {
            comando[i] = '\0';
            break;
        }
    }
    return comando;
}

int esRedireccion(char **tokens) {
    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], ">") == 0)
        {
            // printf("Redireccion detectada\n");
            return i;
        }
    }
    return -1;
}

void agregarPath(char **nuevoPath, int numPaths)
{
    int indice;
    for (indice = 0; paths[indice] != NULL && indice < 100; indice++)
    {
        printf("%d. %s\n", indice, paths[indice]);
    }

    for (int i = indice; (i - indice) < numPaths && i < 100; i++)
    {
        paths[i] = nuevoPath[i - indice + 1];
        printf("%d. %s\n", i, paths[i]);
    }

    printf("PATH agregado correctamente.\n");
}

void error()
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}


void ejecutarComando(char **tokens, int numTokens)
{ // Recibe los tokens y revisa si es un comando prestablecido y sino hace un execv

    if (tokens[0] == NULL)
    {
        printf("No se ingresó ningún comando.\n");
        return;
    }
    else if (strcmp(tokens[0], "exit") == 0 && numTokens == 1)
    {
        printf("Hasta luego...\n");
        exit(0);
    }
    else if (strcmp(tokens[0], "cd") == 0)
    {
        if (tokens[1] == NULL)
        {
            error();
            printf("Error: No se ha especificado el directorio\n");
        }
        else if (numTokens > 2)
        {
            error();
            printf("Error: Demasiados argumentos para cd\n");
        }
        else
        {
            if (chdir(tokens[1]) != 0)
            {
                error();
            }
        }
        return;
    }
    else if (strcmp(tokens[0], "path") == 0)
    {
        if (numTokens >= 2)
        {
            agregarPath(tokens, numTokens - 1);
        }
        else
        {
            error();
        }

        // El profe dice que esto siempre se debe reemplazar, de momento solo se agregan pero no se reemplazan

        // printf("Agregando PATH: %s\n", tokens[1]);
        // int indice;
        // for (indice = 0; paths[indice] != NULL && indice < 100; indice++){
        //   printf("%d. %s\n", indice, paths[indice]);
        // }
        // paths[indice] = tokens[1];
        // printf("PATH agregado correctamente.\n");
    }
    else
    {
        // Delaramos path que va a ser la ruta completa a buscar, se une la que ingresa el usuario con las rutas ya agregadas
        char path[1000];
        pid_t proceso = fork();
        if (proceso < 0)
        {
            error();
        }
        else if (proceso > 0)
        {
            wait(NULL);
            //printf("Proceso hijo terminado\n");

            return;
        }
        else
        {
            //Proceso hijo
            // Revisamos si hay redireccion
            int index = esRedireccion(tokens);

            if (index != -1)
            {
                tokens[index] = NULL; // Cortamos el arreglo para que execv no vea el >
                char *archivoRedireccion = tokens[index + 1];
                int fd = open(archivoRedireccion, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    error();
                    exit(1);
                } else {
                    // Redireccionamos la salida estandar y de error al archivo
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }
            }

            //printf("Proceso hijo ejecutando comando...\n");
            if (strchr(tokens[0], '/') != NULL)
            {
                //printf("El comando incluye ruta\n");
                // Si ya incluye la ruta, intentamos ejectutar con access
                if (access(tokens[0], X_OK) == 0)
                {
                    execv(tokens[0], tokens);
                }
            }
            else
            {
                //printf("Buscando la ruta\n");
                for (int i = 0; paths[i] != NULL; i++)
                {
                    // Concatenamos las rutas con lo que ingresa el usuario
                    sprintf(path, "%s/%s", paths[i], tokens[0]);
                    if (access(path, X_OK) == 0)
                    {
                        //printf("Ejecutando comando en path: %s\n", path);
                        execv(path, tokens);
                        break;
                    }
                    else
                    {
                       // printf("Sin acceso\n");
                    }
                }
                // Matamos el proceso si no se encuentra el comando
                error();
                exit(1);
            }
        }
    }
}

int contArgs(char *comando)
{
    int contador = 1;
    if (comando[0] == '\0')
    {
        // Si el comando está vacío, devolvemos 0
        return 0;
    }
    for (int i = 0; comando[i] != '\0'; i++)
    {
        if (comando[i] == 32)
        {
            // Aumentamos el contador por cada espacio (32 en ASCII)
            contador++;
        }
    }
    return contador;
}
int main(int argc, char *argv[])
{
    int modoEjecucion = 0; // 1 para modo interactivo, 0 para modo por lotes
    // Primero verificamos que modo de ejecucion se va a utilizar
    if (argc > 2)
    {
        printf("Error: Numero de argumentos invalido\n");
        exit(1);
    }
    else if (argc == 1)
    {
        modoEjecucion = 1; // Modo interactivo

        // Directorio del shell inicial
        paths[0] = "/bin";
        while (1)
        {
            printf("gtesh> ");
            char *comando = NULL;
            size_t tamaño = 0;

            // Leemos por consola
            getline(&comando, &tamaño, stdin);
            // Eliminamos el enter para evitar problemas
            comando = eliminarEnter(comando);
            //printf("Comando ingresado: %s\n", comando);

            // Contamos los argumentos, para hacer un arreglo
            int cArgs = contArgs(comando);
            //printf("Numero de argumentos: %d\n", cArgs);

            // Creamos el arreglo de punteros para los argumentos
            char *tokens[cArgs];

            // Separamos el comando en tokens
            // Usamos strsep porque es mas seguro que strtok
            for (int i = 0; i < cArgs; i++)
            {
                // Cuando encuentra un espacio separa el string y guarda el puntero en el arreglo
                tokens[i] = strsep(&comando, " ");
            }
            // El ultimo puntero debe ser NULL para execvp
            tokens[cArgs] = NULL;

            // Esto es solo para debuggear, luego se elimina
            // for (int i = 0; tokens[i] != NULL; i++)
            // {
            //     printf("Token %d: %s\n", i, tokens[i]);
            // }

            ejecutarComando(tokens, cArgs);
        }
    }
    else if (argc == 2)
    {
        modoEjecucion = 0; // Modo por lotes
        
        
    }

    return 0;
}