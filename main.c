#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "hashmap.h"

unsigned long getLength(char* str) {
    unsigned long i = 0;
    while(str[i] != '\0') i++;
    return i;
}

char* getExtencion(char* path) {
    unsigned long i = 0;
    unsigned long last = 0;

    while(path[i++] != '\0') 
        if(path[i] == '.') last = i;

    return path + last;
}

void customRegular(char* path, struct hashmap_s* colors,
        struct hashmap_s* icons, char** icon, char** color) {
    char* extencion = getExtencion(path);
    char* gcolor = (char*) hashmap_get(colors, extencion, strlen(extencion));
    if(gcolor != NULL) {
        *color = gcolor;
    }

    char* gicon = (char*) hashmap_get(icons, extencion, strlen(extencion));
    if(gicon == NULL) {
        *icon = "\uf016";
    } else {
        *icon = gicon;
    }
}

int main(){
    struct winsize win;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == -1) {
        perror("ioctl");
        return -1;
    }

    struct hashmap_s regColors;
    if (0 != hashmap_create(5, &regColors)) {
        // error!
    }

    if(0 != hashmap_put(&regColors, ".html", strlen(".html"), "234;128;111")) {
        // error!
    }

    if(0 != hashmap_put(&regColors, ".rs", strlen(".rs"), "200;148;71")) {
        // error!
    }
    
    if(0 != hashmap_put(&regColors, ".py", strlen(".py"), "230;190;120")) {
        // error!
    }

    if(0 != hashmap_put(&regColors, ".c", strlen(".c"), "240;160;140")) {
        // error!
    }

    struct hashmap_s regIcons;
    if(0 != hashmap_create(5, &regIcons)) {
        // error!
    }

    if(0 != hashmap_put(&regIcons, ".html", strlen(".html"), "\ue60e")) {
        // error!
    }

    if(0 != hashmap_put(&regIcons, ".rs", strlen(".rs"), "\ue7a8")) {
        // error!
    }

    if(0 != hashmap_put(&regIcons, ".py", strlen(".py"), "\ue73c")) {
        // error!
    }

    if(0 != hashmap_put(&regIcons, ".pepe", strlen(".pepe"), "\ue2a8")) {
        // error!
    }

    if(0 != hashmap_put(&regIcons, ".txt", strlen(".txt"), "\uf15c")) {
        // error!
    }

    if(0 != hashmap_put(&regIcons, ".c", strlen(".c"), "\ue61e")) {
        // error!
    }

    //printf("Size:\n\t%d:%d\n", win.ws_col, win.ws_row);

    DIR* dir = opendir(".");
    if(dir == NULL) return 1;

    struct dirent* entry;
    unsigned short margins = 2;
    unsigned short n = margins;
    _Bool startFlag = 1;
    unsigned short spaceBtwn = 2;

    while((entry = readdir(dir)) != NULL) {
        unsigned long name_len = getLength(entry->d_name) + 2;

        if(n + name_len + spaceBtwn >= win.ws_col + 1 - margins || startFlag) {
            if(!startFlag) printf("\n");

            if(startFlag) startFlag = 0;
            n = margins;

            for(int i = 0; i < margins; i++) {
                printf("\e[C");
            }
        }

        char* color = NULL;
        char* icon = NULL;
        _Bool hidden = entry->d_name[0] == '.';

        if(hidden && 
                !(strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))) hidden = 0;

        struct stat info;
        stat(entry->d_name, &info);
         
        if(S_ISDIR(info.st_mode)) {
            icon = "\ue5ff";
            if(strcmp(entry->d_name, "Alezandra") == 0) icon = "\ue26a";
        } else if(info.st_mode & S_IXUSR) {
            icon = "\uea86";
            color = "100;180;230";
        } else {
            customRegular(entry->d_name, &regColors, &regIcons, &icon, &color);
        }
        
        if(hidden) printf("\e[2m");
        if(color != NULL) printf("\e[38;2;%sm", color);
        if(icon != NULL) printf("%s ", icon);

        printf("%s\e[0m  ", entry->d_name);

        n += name_len + spaceBtwn;
    }
    printf("\n");

    hashmap_destroy(&regColors);
    hashmap_destroy(&regIcons);
    closedir(dir);
    return 0;
}
