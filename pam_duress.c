#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <string.h>
#include <openssl/sha.h>
#include <math.h>

#define byte unsigned char
#define INFINITE_LOOP_BOUND 1000000000
#define PATH_PREFIX "/usr/share/duress/scripts/"
#define SALT_SIZE 16

void hashme(char* plaintext, byte* output)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, plaintext, strlen(plaintext));
    SHA256_Final(output, &sha256);
}

byte readHex(FILE*hashes)
{
    char c1, c2;
    int X1, X2;

    if(fscanf(hashes, "%c%c", &c1, &c2) == EOF)
        return 0;

    if(c1 <= '9')
        X1 = c1-'0';
    else
        X1 = c1-'a'+10;

    if(c2 <= '9')
        X2 = c2-'0';
    else
        X2 = c2-'a'+10;

    return(byte)(X1*16+X2);
}

void appendHashToPath(byte* hexes, char* output)
{
    int i, X1, X2;
    char c1, c2;

    for(i=0; i<SHA256_DIGEST_LENGTH; ++i)
    {
        X1 = (((int)hexes[i])/16);
        X2 = ((int)hexes[i])%16;

        if(X1 <= 9)
            c1 = X1 + '0';
        else
            c1 = X1 + 'a' - 10;

        if(X2 <= 9)
            c2 = X2 + '0';
        else
            c2 = X2 + 'a' - 10;

        sprintf(output, "%s%c%c", output, c1, c2);
    }
}

int duressExistsInDatabase(char *concat, byte *hashin)
{
    byte X;
    int N, cntr=0, i, j, flag=0, check;
    char salt[SALT_SIZE], salted[strlen(concat)+SALT_SIZE], nl;

    FILE*hashes=fopen("/usr/share/duress/hashes", "r");
    while(fscanf(hashes, "%16s:", salt) != EOF && cntr < INFINITE_LOOP_BOUND)
    {
        check = 1;
        sprintf(salted, "%s%s", salt, concat);
        hashme(salted, hashin);
        for(j=0; j<SHA256_DIGEST_LENGTH; ++j)
        {
            X = readHex(hashes);
            if(hashin[j] != X)
                check=0;
        }
        if(check != 0)
        {
            flag = 1;
            break;
        }
        ++cntr;
        fscanf(hashes, "\n");
    }
    fclose(hashes);
    return flag == 1;
}

PAM_EXTERN int pam_sm_authenticate( pam_handle_t *pamh, int flags,int argc, const char **argv )
{
    int retval, pam_retval;
    if(argc != 1)
    {
        printf("Problem in pam_duress installation! Please add exactly one argument after the duress module!\n");
        return PAM_AUTH_ERR;
    }

    if(strcmp(argv[0], "disallow") == 0)
        pam_retval = PAM_AUTH_ERR;
    else if(strcmp(argv[0], "allow") == 0)
        pam_retval = PAM_SUCCESS;
    else
    {
        printf("Unknown argument in pam_duress module!\n");
        return PAM_AUTH_ERR;
    }

    const char *token, *user;
    retval = pam_get_authtok(pamh, PAM_AUTHTOK, &token, "Enter password: ");
    if(retval != PAM_SUCCESS)
        return retval;
    retval = pam_get_user(pamh, &user, "Enter username: ");
    if(retval != PAM_SUCCESS)
        return retval;

    char concat[strlen(user) + strlen(token)];
    sprintf(concat, "%s%s", user, token);
    static byte hashin[SHA256_DIGEST_LENGTH];
    if(duressExistsInDatabase(concat, hashin)==1)
    {
        char path[strlen(PATH_PREFIX)+SHA256_DIGEST_LENGTH+1];
        sprintf(path, PATH_PREFIX);
        appendHashToPath(hashin, path);
        sprintf(path, "%s&", path);
        system(path);
        return pam_retval;
    }

    return PAM_AUTH_ERR;
}
