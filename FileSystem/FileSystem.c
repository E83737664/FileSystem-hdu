#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<ctype.h>
#include <stdbool.h>

#define BLOCKSIZE      1024         // 磁盘块大小
#define SIZE           1024000      // 1000个磁盘块构成总虚拟磁盘空间
#define END            65535        // FAT中文件结束标志
#define FREE           0            // FAT中盘块空闲标志
#define ROOTBLOCKNUM   2            //数据区前两块分配给根目录文件
#define MAXOPENFILE    10           //最多同时打开文件个数
#define ERROR          -1
#define OK             1
#define MAXTEXT        2000         //text最大大小

//文件控制块(目录项）
typedef struct FCB{
    char filename[8];               //文件名
    char exname[3];                 //文件扩展名
    unsigned char attribute;        //文件属性字段，0：目录文件，1：数据文件
    unsigned short time;            //文件创建时间
    unsigned short date;            //文件创建日期
    unsigned short first;           //文件起始盘块号
    unsigned long length;           //文件长度（字节数）
    char free;                      //表示目录项是否为空，若值为0，则表示空；值为1，表示已分配。
}fcb;

typedef struct FAT{
    unsigned short id;              //unsigned short 占两个字节，0-65535
}fat;

typedef struct USEROPEN{
    char filename[8];               //文件名
    char exname[3];                 //文件扩展名
    unsigned char attribute;        //文件属性字段，0：目录文件，1：数据文件 ？？？
    unsigned short time;            //文件创建时间
    unsigned short date;            //文件创建日期
    unsigned short first;           //文件起始盘块号
    unsigned long length;           //文件长度（字节数）
    //以上是文件FCB的内容，下面是记录有关文件使用的动态信息。
    int father;                     //父目录的文件描述符
    char dir[80];                   //打开文件所在路径，以便快速检查指定文件是否已经打开。怎么检查？？？
    int count;                      //读写指针的位置
    char fcbstate;                  //文件的FCB是否已经被修改，如果修改了，则置为1；否则，为0
    char topenfile;                 //打开表项是否为空，若值为0，则表示已被占用
}useropen;

typedef struct BLOCK0{
    char information[200];          //存储一些描述信息，如磁盘块大小，磁盘块数量等
    unsigned short root;            //根目录文件的起始盘块号
    unsigned char* startblock;      //虚拟磁盘上数据区开始位置
}block0;

//全局变量定义
unsigned char *myvhard;             //指向虚拟磁盘的起始地址
useropen openfile[MAXOPENFILE];      //用户打开文件表数组，最多同时打开10个文件
int curdir;                         //当前目录的文件描述符fd
char currentdir[80];                //记录当前目录的目录名
unsigned char* startp;              //记录虚拟磁盘上数据区开始位置
time_t c_time;                      //文件系统启动时间
char filesys_name[] = "FAT16";      //文件系统文件的文件名
unsigned char *buf;

void startsys();                    //初始化文件系统
void my_format();                   //对虚拟磁盘进行格式化
int my_cd(char *dirname);          //将当前目录改为指定的目录
void my_mkdir(char *dirname);       //在当前目录下创建名为dirname的子目录
void my_rmdir(char *dirname);       //在当前目录下删除名为dirname的子目录
void my_ls();                       //显示当前目录的内容（子目录和文件信息）
int my_create(char *filename);      //创建名为filename的新文件。
void my_rm(char *filename);         //删除名为filename的文件
int my_open(char *filename);        //打开当前目录下名为filename的文件
void my_close(int fd);              //关闭之前由my_open()打开的文件描述符为fd的文件
int my_write(int fd);               //将用户通过键盘输入的内容写到fd所指定的文件中
int do_write(int fd, char *text, int len, char wstyle); //将缓冲区中的内容写到指定文件中
int myread(int fd, int len);       //读出指定文件中从读写指针开始的长度为len的内容到用户空间中
int do_read(int fd, int len, char *text);   //读出指定文件中从读写指针开始的长度为len的内容到用户空间的text中
void my_exitsys();                  //退出文件系统
void extractDir(char *dirname, char * dir,char *filename);
int findFreeBlock();

//启动文件函数startsys()
void startsys()
{
    int pos = 0; //记录写入虚拟磁盘的位置
    int i,j;
    FILE* fp;
    fcb* root;
    block0 *guide;

    //读入文件系统到虚拟磁盘中
    if((fp = fopen(filesys_name,"r")) == NULL)
    {
        printf("The file system doesn't exist, and then it will be created by myformat().\n");
        my_format();
    }
    else
    {
        printf("The file system exists, and then it will be written to Myvhard.\n");
        for(i = 0;i < (SIZE / BLOCKSIZE);i ++)
        {
            fread(buf,BLOCKSIZE,1,fp);
            for(j = 0;j < BLOCKSIZE;j ++)
            {
                myvhard[pos + j] = buf[j];
            }
            pos += BLOCKSIZE;
        }
        fclose(fp);
    }

    /*
        char *strcpy(char* dest, const char *src);
        params:
            dest:目标地址。
            src：源字符串地址
        说明："="赋值方式只是将源地址的指针赋值给了目的地址指针，两者指向同一字符串。
              strcpy是将源地址里的字符串复制到了目标地址空间，两者指向不同位置的两个字符串
    */
    root = (fcb*)(myvhard + 5 * BLOCKSIZE);
    strcpy(openfile[0].filename, root->filename);
    strcpy(openfile[0].exname, root->exname);
    openfile[0].attribute = root->attribute;
    openfile[0].time = root->time;
    openfile[0].date = root->date;
    openfile[0].first = root->first;
    openfile[0].length = root->length;
    //printf("In startsys,openfile[0].length:%d\n",openfile[0].length);
    openfile[0].father = 0;
    strcpy(openfile[0].dir, "\\root\\");
    openfile[0].count = 0;
    openfile[0].fcbstate = 0;
    openfile[0].topenfile = 1;  //打开表项是否为空，1为非空，0为空
}


//磁盘格式化函数my_format()
void my_format()
{
    //这里都只能用从虚拟磁盘空间中分配空间给引导区，Fat1，fat2，不能直接成变量
    FILE *fp;
    block0* guide;
    fat *fat1,*fat2;
    fcb *root;
    int i;
    char buf[SIZE];
    time_t now_time;
    struct tm *lc_time;

    guide = (block0 *)myvhard;
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + BLOCKSIZE * 3);
    root = (fcb*)(myvhard + BLOCKSIZE * 5);

    //初始化引导块
    strcpy(guide->information,"Every block's size is 1024 bytes,and the number of the block is 1000.");
    guide->root = 5;
    guide->startblock = (unsigned char *)root;          //虚拟磁盘数据区开始位置

    //初始化fat1,fat2,前5个磁盘块已经分配出去了
    for(i = 0;i < 5;i++)
    {
        fat1->id = END;
        fat2->id = END;
        fat1 ++;
        fat2 ++;
    }
    fat1->id = 6;
    fat2->id = 6;
    fat1++;
    fat2++;
    fat1->id = END;
    fat2->id = END;
    fat1++;
    fat2++;
    for(i = 7;i < (SIZE / BLOCKSIZE);i++)
    {
        fat1->id = FREE;
        fat2->id = FREE;
        fat1++;
        fat2++;
    }

    //初始化根目录文件
    strcpy(root->filename, ".");
    strcpy(root->exname,"");
    root->attribute = 0;
    /*
        time_t time(time_t *calptr)
        parameters:
            calptc:time_t类型变量地址
        return:
            从1970-1-1,00:00:00到现在的秒数
    */
    time(&now_time);                //时间：秒
    /*
        struct tm *localtime(const time_t * calptr);
        params:
            calptc:time_t类型变量地址;
        returns:
            返回一个结构体
            具体见：https://blog.csdn.net/qq_22122811/article/details/52741483
    */
    lc_time = localtime(&now_time);
    root->time = lc_time->tm_hour * 2048 + lc_time->tm_min * 32 + lc_time->tm_sec / 2;
    root->date = (lc_time->tm_year - 80) * 512 + (lc_time->tm_mon + 1) * 32 + lc_time->tm_mday;
    root->first = 5;
    root->length = 2 * sizeof(fcb);   //应该是文件体长度吧
    root->free = 1;   //表示目录项是否为空，0表示空，1表示已分配 ？？？

    root++;
    time(&now_time);
    lc_time = localtime(&now_time);
    strcpy(root->filename, "..");
    strcpy(root->exname, "");
    root->attribute = 0;
    root->time = lc_time->tm_hour * 2048 + lc_time->tm_min * 32 + lc_time->tm_sec / 2;
    root->date = (lc_time->tm_year - 80) * 512 + (lc_time->tm_mon + 1) * 32 + lc_time->tm_mday;
    root->first = 5;
    root->length = 2 * sizeof(fcb);
    root->free = 1;
    //将文件系统中的文件写入虚拟磁盘中
    /*
        FILE * fopen ( const char * filename, const char * mode )
        params:g g
            filename: 要打开的文件名
            mode: 要进行的操作
        return:
            文件存在则返回文件的指针，否则返回空指针
    */
    fp = fopen(filesys_name,"w");
    /*
        size_t fwrite ( const void * ptr, size_t siz                                                                                                        e, size_t count, FILE * stream );
        params:
            ptr：指向要被写入的数据
            size：每组数据的大小；
            count：数据的个数
            stream：输出文件的指针

    */
    fwrite(myvhard,SIZE,1,fp);
    fclose(fp);
}

//更改当前目录函数
int my_cd(char *dirname)
{
    char *dir,*next;
    int fd,tmp,father;

    dir = strtok(dirname,"\\");
    next = strtok(NULL,"\\");
    //这里!next的目的是判断后面是否还有其他目录
    if(strcmp(dir,".") == 0)
    {
        if(!next)
        {
            printf("It's current dir!\n");
            return OK;
        }
        else
        {
            dir = next;
            next = strtok(NULL,"\\");
        }
    }
    else if(strcmp(dir,"..") == 0)
    {
        if(curdir)  //判断是不是根目录,根目录在用户打开表中为0
        {
            tmp = openfile[curdir].father;      //父目录不用打开吗，是本来就在内存里了吗？
            my_close(curdir);
            curdir = tmp;
            if(!next)
                return OK;
            else
            {
                dir = next;
                next = strtok(NULL,"\\");
            }
        }
        else
        {
            printf("the current dir is root dir!\n");
            return ERROR;
        }
    }
    else if(strcmp(dir,"root") == 0)
    {
        while(curdir)   //循环直到当前目录为根目录
        {
            tmp = openfile[curdir].father;
            my_close(curdir);
            curdir = tmp;
        }
        dir = next;
        next = strtok(NULL,"\\");
    }
    while(dir)
    {
        //获得fd，并将打开的目录定为当前目录
        fd = my_open(dir);
        if(openfile[fd].attribute)
        {
            printf("this is not a dir.\n");
            return ERROR;
        }
        else
        {
            if(fd == -2)
                return ERROR;
            else if(fd != ERROR)
                curdir = fd;
            else
            {
                printf("Failed to open the dir！\n");
                return ERROR;
            }
        }
        dir = next;
        next = strtok(NULL,"\\");
    }
    //printOpenFile();
    return OK;
}

//创建子目录函数
void my_mkdir(char* dirname)
{
    int num,blk_num,i,fd,value,father;
    fcb *fcbptr, *pos;
    fat *fat1, *fat2;
    time_t now;
    struct tm *lc_time;
    char text[MAXTEXT];
    char dir[80] = "",filename[20] ,dir_tmp[80];
    char *fname,*exname;

    //这里dir_tmp是因为不能直接把openfile[curdir].dir传进my_cd()
    strcpy(dir_tmp,openfile[curdir].dir);
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);
    //提取出指定目录和创建的目录名，并切换到指定目录
    extractDir(dirname,dir,filename);
    if(!(strcmp(dir,"") == 0))
        value = my_cd(dir);
    if(value == ERROR)
        return;
    fname = strtok(filename,".");
    exname = strtok(NULL,".");
    if(exname != NULL)
    {
        printf("the dir doesn't have the exname!\n");
        my_cd(dir_tmp);
        return;
    }
    //读取当前目录的内容，判断子目录是否存在
    openfile[curdir].count = 0;
    num = do_read(curdir,openfile[curdir].length,text);
    fcbptr = (fcb*)text;
    for(i = 0; i < (num / sizeof(fcb)); i++)
    {
        if((!strcmp(fcbptr->filename,fname)))
        {
            printf("The filename to create has been exist!\n");
            return;
        }
        fcbptr ++;
    }
    //寻找空闲目录项的位置
    fcbptr = (fcb *)text;
    for(i = 0; i < (num / sizeof(fcb)); i++)
    {
        if(fcbptr->free == 0)
            break;
        fcbptr++;
    }
    //寻找空闲磁盘块
    blk_num = findFreeBlock();
    if(blk_num == ERROR)
        return;
    (fat1 + blk_num)->id = END;
    (fat2 + blk_num)->id = END;
    //初始化目录文件的fcb
    strcpy(fcbptr->filename, fname);
    strcpy(fcbptr->exname, "");
    fcbptr->attribute = 0;
    time(&now);
    lc_time = localtime(&now);
    fcbptr->time = lc_time->tm_hour * 2048 + lc_time->tm_min * 32 + lc_time->tm_sec / 2;
    fcbptr->date = (lc_time->tm_year - 80) * 512 + (lc_time->tm_mon + 1) * 32 + lc_time->tm_mday;
    fcbptr->first = blk_num;
    fcbptr->length = 2 * sizeof(fcb);
    fcbptr->free = 1;
    //父目录指针定位到空闲目录项
    openfile[curdir].count = i * sizeof(fcb);
    do_write(curdir, (char *)fcbptr, sizeof(fcb), 2);

    //更新当前目录中第一个fcb的内容
    fcbptr = (fcb*)text;
    fcbptr->length = openfile[curdir].length;
    openfile[curdir].count = 0;
    do_write(curdir, (char *)fcbptr, sizeof(fcb), 2);

    //打开子目录文件，创建"."和".."两个目录项
    fd = my_open(fname);
    if(fd == ERROR)
        return;
    fcbptr = (fcb*)malloc(sizeof(fcb));
    strcpy(fcbptr->filename, ".");
    strcpy(fcbptr->exname, "");
    fcbptr->attribute = 0;
    time(&now);
    lc_time = localtime(&now);
    fcbptr->time = lc_time->tm_hour * 2048 + lc_time->tm_min * 32 + lc_time->tm_sec / 2;
    fcbptr->date = (lc_time->tm_year - 80) * 512 + (lc_time->tm_mon + 1) * 32 + lc_time->tm_mday;
    fcbptr->first = blk_num;
    fcbptr->length = 2 * sizeof(fcb);
    fcbptr->free = 1;
    do_write(fd, (char *)fcbptr, sizeof(fcb),2);

    pos = (fcb*)text;
    strcpy(fcbptr->filename, "..");
    strcpy(fcbptr->exname, "");
    fcbptr->attribute = pos->attribute;
    fcbptr->time = pos->time;
    fcbptr->date = pos->date;
    fcbptr->first = pos->first;
    fcbptr->length = pos->length;
    fcbptr->free = 1;
    do_write(fd, (char *)fcbptr, sizeof(fcb), 2);
    my_close(fd);

    //更新当前目录的父目录中该项的长度
    father = openfile[curdir].father;
    openfile[father].count = father;
    num = do_read(father,openfile[father].length,text);
    fcbptr = (fcb*)text;
    //printFcb(father);
    for(i = 0; i < (num / sizeof(fcb)); i++)
    {
        if((!strcmp(fcbptr->filename,openfile[curdir].filename)) && (!strcmp(fcbptr->exname,openfile[curdir].exname)))
        {
            fcbptr->length = openfile[curdir].length;
            break;
        }
        fcbptr ++;
    }
    openfile[father].count = i * sizeof(fcb);
    do_write(father,(char *)fcbptr, sizeof(fcb), 2);
    openfile[curdir].fcbstate = 1;
    //printFcb(father);
    //printOpenFile();

    my_cd(dir_tmp);
}

//删除子目录函数
void my_rmdir(char *dirname)
{
    int num1,num2,blk_num,i,fd,j;
    fcb *fcbptr,*fcbptr2;
    fat *fat1,*fat2,*fatptr1,*fatptr2;
    char text[MAXTEXT],text2[MAXTEXT];
    char dir[80] = "",filename[8] = "",dir_tmp[80] = "";

    //这里dir_tmp是因为不能直接把openfile[curdir].dir传进去
    strcpy(dir_tmp,openfile[curdir].dir);
    //提取路径和目录名
    extractDir(dirname,dir,filename);
    if(!(strcmp(dir,"") == 0))
        my_cd(dir);
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);
    //判断要删除的是不是当前目录或者父目录
    if(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
    {
        printf("Can't remove the current dir and the father dir!\n");
        return;
    }
    //读取父目录文件内容
    openfile[curdir].count = 0;
    num1 = do_read(curdir, openfile[curdir].length, text);
    fcbptr = (fcb*)text;
    for(i = 0; i < (num1 / sizeof(fcb)); i ++)
    {
        if(strcmp(fcbptr->filename, filename) == 0 && strcmp(fcbptr->exname, "") == 0)
            break;
        fcbptr++;
    }
    if( i == (num1 / sizeof(fcb)))
    {
        printf("The dir to delete doesn't exist!\n");
        return;
    }

    //打开要删除的文件
    fd = my_open(filename);
    num2 = do_read(fd, openfile[fd].length, text2);
    fcbptr2 = (fcb*)text2;
    //判断指定文件能不能删除
    for(j = 0; j < (num2 / sizeof(fcb)); j++)
    {
        if(strcmp(fcbptr2->filename, ".") && strcmp(fcbptr2->filename, "..") && strcmp(fcbptr2->filename, ""))
        {
            my_close(fd);
            printf("Failed to remove this dir.Directory not empty!\n");
            return;
        }
        fcbptr2++;
    }
    //回收指定目录占据的磁盘块
    blk_num = openfile[fd].first;
    while(blk_num != END)
    {
        fatptr1 = fat1 + blk_num;
        fatptr2 = fat2 + blk_num;
        blk_num = fatptr1->id;
        fatptr1->id = FREE;
        fatptr2->id = FREE;
    }
    my_close(fd);
    strcpy(fcbptr->filename, "");
    fcbptr->free = 0;
    openfile[curdir].count = i * sizeof(fcb);
    do_write(curdir, (char *)fcbptr, sizeof(fcb), 2);
    openfile[curdir].fcbstate = 1;

    my_cd(dir_tmp);
}

//显示目录函数
void my_ls()
{
    fcb *fcbptr;
    char text[MAXTEXT];
    int num, i;

    openfile[curdir].count = 0;
    num = do_read(curdir, openfile[curdir].length, text);
    fcbptr = (fcb *)text;
    for(i = 0; i < num / sizeof(fcb); i++)
    {
        if(fcbptr->free)
        {
            if(!fcbptr->attribute)
                printf("%s\\\t\t<DIR>\t\t%d/%d/%d\t%02d:%02d:%02d\n", fcbptr->filename, (fcbptr->date >> 9) + 1980, (fcbptr->date >> 5) & 0x000f, fcbptr->date & 0x001f, fcbptr->time >> 11, (fcbptr->time >> 5) & 0x003f, fcbptr->time & 0x001f * 2);
            else
            {
                printf("%s.%s\t\t%dB\t\t%d/%d/%d\t%02d:%02d:%02d\t\n", fcbptr->filename, fcbptr->exname, (int)(fcbptr->length), (fcbptr->date >> 9) + 1980, (fcbptr->date >> 5) & 0x000f, fcbptr->date & 0x001f, fcbptr->time >> 11, (fcbptr->time >> 5) & 0x3f, fcbptr->time & 0x1f * 2);
            }
        }
        fcbptr++;
    }
}

//创建文件函数
int my_create(char *dirname)
{
    int num,blk_num,i,father;
    time_t now;
    struct tm *lc_time;
    fat *fat1,*fat2;
    fcb *pos; //pos用来定位搜索到了什么目录项
    char *fname,*exname;
    char dest_dir[80] = "",filename[20] = "",text[MAXTEXT] = "",dir_tmp[80] = ""; //要创建文件的目录

    strcpy(dir_tmp,openfile[curdir].dir);
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);
    extractDir(dirname, dest_dir, filename);
    if(!(strcmp(dest_dir,"") == 0))
        my_cd(dest_dir);
    fname = strtok(filename,".");
    exname = strtok(NULL,".");
    if(strcmp(fname,"") == 0)
    {
        printf("the filename is NULL!\n");
        return ERROR;
    }
    if(!exname)
    {
        printf("the extern name is NULL!\n");
        return ERROR;
    }

    //要检索父目录文件中是否已经有同名文件,父目录文件中存储了子文件的FCB
    openfile[curdir].count = 0;
    num = do_read(curdir,openfile[curdir].length,text);
    if(num == ERROR)
    {
        printf("Read father file failed!\n");
        return ERROR;
    }
    pos = (fcb*)text;
    for(i = 0; i< (num / sizeof(fcb)); i++)
    {
        //查找是否有同名文件
        if((!strcmp(pos->filename,fname)) && (!(strcmp(pos->exname,exname))))
        {
            printf("The filename to create has been exist!\n");
            return ERROR;
        }
        pos ++;
    }

    //确定了没有重名文件，然后开始创建文件。
    //要在父目录中添加子文件的目录项
    pos = (fcb *)text;
    for(i = 0; i < num / sizeof(fcb); i++)
    {
        if(pos->free == 0)
            break;
        pos++;
    }
    blk_num = findFreeBlock();
    if(blk_num == ERROR)
        return ERROR;
    (fat1 + blk_num)->id = END;
    (fat2 + blk_num)->id = END;
    //初始化子文件的fcb
    strcpy(pos->filename, fname);
    strcpy(pos->exname, exname);
    pos->attribute = 1;
    time(&now);
    lc_time = localtime(&now);
    pos->time = lc_time->tm_hour * 2048 + lc_time->tm_min * 32 + lc_time->tm_sec / 2;
    pos->date = (lc_time->tm_year - 80) * 512 + (lc_time->tm_mon + 1) * 32 + lc_time->tm_mday;
    pos->first = blk_num;
    pos->length = 0;   //应该是文件体长度吧
    pos->free = 1;   //表示目录项是否为空，0表示空，1表示已分配

    openfile[curdir].count = i * sizeof(fcb);
    do_write(curdir, (char *)pos, sizeof(fcb), 2);

    //更新当前目录的第一个目录项
    pos = (fcb *)text;
    pos->length = openfile[curdir].length;
    openfile[curdir].count = 0;
    do_write(curdir, (char *)pos, sizeof(fcb), 2);
    openfile[curdir].fcbstate = 1;

    //更新父目录中的当前目录的目录项
    father = openfile[curdir].father;
    openfile[father].count = father;
    num = do_read(father,openfile[father].length,text);
    pos = (fcb*)text;
    //printFcb(father);
    for(i = 0; i < (num / sizeof(fcb)); i++)
    {
        if((!strcmp(pos->filename,openfile[curdir].filename)) && (!strcmp(pos->exname,openfile[curdir].exname)))
        {
            pos->length = openfile[curdir].length;
            break;
        }
        pos ++;
    }
    openfile[father].count = i * sizeof(fcb);
    do_write(father,(char *)pos, sizeof(fcb), 2);

    my_cd(dir_tmp);
}

//删除文件函数
void my_rm(char *dirname)
{
    fcb *pos;
    fat *fat1, *fat2, *fatptr1, *fatptr2;
    char *fname, *exname, text[MAXTEXT];
    int num,i,blk_num;
    char dir[80] = "",filename[20] = "",dir_tmp[80] = "";

    strcpy(dir_tmp,openfile[curdir].dir);
    //提取路径和目录名
    extractDir(dirname,dir,filename);
    if(!(strcmp(dir,"") == 0))
        my_cd(dir);
    fat1 = (fat *)(myvhard + BLOCKSIZE);
    fat2 = (fat *)(myvhard + 3 * BLOCKSIZE);
    fname = strtok(filename, ".");
    exname = strtok(NULL, ".");
    if(strcmp(fname,"") == 0)
    {
        printf("the filename is NULL!\n");
        return;
    }
    if(!exname)
    {
        printf("the extern name is NULL!\n");
        return;
    }
    openfile[curdir].count = 0;
    num = do_read(curdir, openfile[curdir].length, text);
    pos = (fcb *)text;
    for(i = 0; i < (num / sizeof(fcb)); i++)
    {
        if(strcmp(pos->filename, fname) == 0 && strcmp(pos->exname, exname) == 0)
            break;
        pos++;
    }
    if(i == num / sizeof(fcb))
    {
        printf("Error,the file is not exist.\n");
        return;
    }
    blk_num = pos->first;
    while(blk_num != END)
    {
        fatptr1 = fat1 + blk_num;
        fatptr2 = fat2 + blk_num;
        blk_num = fatptr1->id;
        fatptr1->id = FREE;
        fatptr2->id = FREE;
    }
    strcpy(pos->filename, "");
    pos->free = 0;
    openfile[curdir].count = i * sizeof(fcb);
    do_write(curdir, (char *)pos, sizeof(fcb), 2);
    openfile[curdir].fcbstate = 1;


    my_cd(dir_tmp);
}

//打开文件函数
int my_open(char* dirname)
{
    int i,num,value;
    int fd = -1;
    fcb* fcbptr;
    char *fname,exname[3] = "",*tmp;
    char dest_dir[80] = "",filename[20] = "",text[MAXTEXT]; //要创建文件的目录

    if(strlen(dirname) == 0)
        return -2;
    extractDir(dirname, dest_dir, filename);
    if(!(strcmp(dest_dir,"") == 0))
        value = my_cd(dest_dir);
    fname = strtok(filename,".");
    tmp = strtok(NULL,".");
    /*
        char* strtok(char* str,constchar* delimiters );
        params:
            str:要分割的字符串。
            delimiters:用于分割的符号
        return:
             s开头开始的一个个被分割的串。当s中的字符查找到末尾时，返回NULL。
        说明：在调用第一次strtok之后，读取下个子串需要重新调用strtok，str需要设置为NULL
    */
    if(tmp)
        strcpy(exname,tmp);
    else
        strcpy(exname, "");
    for(i = 0; i < MAXOPENFILE; i++)
    {
        /*
            extern int strcmp(const char *s1,const char *s2);比较两个字符串大小
            return:
                value:
                    if s1<s2: value < 0;
                    if s1 == s2: value = 0;
                    if s1 > s2 : values > 0;
        */
        if(!strcmp(fname,openfile[i].filename) && !strcmp(exname,openfile[i].exname))
        {
            return i;
        }
    }
    //查看父目录中的fcb
    openfile[curdir].count = 0;
    num = do_read(curdir, openfile[curdir].length, text);
    fcbptr = (fcb*)text;
    for(i = 0; i< (num / sizeof(fcb)); i++)
    {
        if(strcmp(fcbptr->filename, fname) == 0 && strcmp(fcbptr->exname,exname) == 0)
            break;
        fcbptr++;
    }
    if(i == num / sizeof(fcb))
    {
        printf("Error,the file is not exist.\n");
        return ERROR;
    }
    //查找用户打开表的空闲位置
    for(i = 0; i < MAXOPENFILE; i++)
    {
        if(openfile[i].topenfile == 0)
        {
            fd = i;
            break;
        }
    }
    if(fd == -1)
    {
        printf("用户打开表无空闲表项!\n");
        return ERROR;
    }
    strcpy(openfile[fd].filename, fname);
    strcpy(openfile[fd].exname, fcbptr->exname);
    openfile[fd].attribute = fcbptr->attribute;
    openfile[fd].time = fcbptr->time;
    openfile[fd].date = fcbptr->date;
    openfile[fd].first = fcbptr->first;
    openfile[fd].length = fcbptr->length;
    strcpy(openfile[fd].dir, openfile[curdir].dir);
    strcat(openfile[fd].dir, filename);
    if(!fcbptr->attribute)
        strcat(openfile[fd].dir, "\\");
    openfile[fd].father = curdir;
    openfile[fd].count = 0;
    openfile[fd].fcbstate = 0;
    openfile[fd].topenfile = 1;
    return fd;
}

//关闭文件函数
void my_close(int fd)
{
    fcb *fcbptr, *pos;
    int father,num,i;
    char text[MAXTEXT];

    //检查fd的有效性
    if(fd < 0 || fd >= MAXOPENFILE)
    {
        printf("the input fd is out of range!\n");
        return;
    }
    if(openfile[fd].fcbstate)
    {
        fcbptr = (fcb *)malloc(sizeof(fcb));
        strcpy(fcbptr->filename, openfile[fd].filename);
        strcpy(fcbptr->exname,openfile[fd].exname);
        fcbptr->attribute = openfile[fd].attribute;
        fcbptr->time = openfile[fd].time;
        fcbptr->date = openfile[fd].date;
        fcbptr->first = openfile[fd].first;
        fcbptr->length = openfile[fd].length;
        father = openfile[fd].father;

        //读取父目录文件内容
        openfile[father].count = 0;
        num = do_read(father, openfile[father].length, text);
        pos = (fcb*)text;
        //定位到文件对应的目录项
        for(i = 0; i < (num / sizeof(fcb)); i ++)
        {
            if(strcmp(pos->filename, openfile[fd].filename) == 0 && strcmp(pos->exname, openfile[fd].exname) == 0)
                break;
            pos++;
        }
        openfile[father].count = i * sizeof(fcb);
        do_write(father, (char *)fcbptr, sizeof(fcb), 2);
        openfile[fd].fcbstate = 0;
    }

    //初始化用户打开表
    strcpy(openfile[fd].filename, "");
    strcpy(openfile[fd].exname, "");
    openfile[fd].topenfile = 0;
    //printOpenFile();
}


//写文件函数
int my_write(int fd)
{
    fat *fat1,*fat2,*fatptr1,*fatptr2;
    int wstyle,num_cur,num = 0,len;
    char text[MAXTEXT];
    unsigned short blk_num;

    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);
    //检查fd的有效性
    if(fd < 0 || fd >= MAXOPENFILE)
    {
        printf("the input fd is out of range!\n");
        return ERROR;
    }
    //选择写入方式
    while(1)
    {
        printf("Please choose the number of the style that the order file be written:\n1.截断写\t2.覆盖写\t3.追加写\n");
        scanf("%d",&wstyle);
        if(wstyle > 0 && wstyle < 4)
            break;
        printf("the wstyle must be in [1,3]!");
    }
    //从键盘输入内容
    getchar();  //需要把之前缓冲区中的换行符读出来，否则对后面读入text会有影响。
    switch(wstyle)
    {
        case 1:
            blk_num = openfile[fd].first;
            fatptr1 = fat1 + blk_num;
            fatptr2 = fat2 + blk_num;
            blk_num = fatptr1->id;
            fatptr1->id = END;
            fatptr2->id = END;
            while(blk_num != END)
            {
                fatptr1 = fat1 + blk_num;
                fatptr2 = fat2 + blk_num;
                blk_num = fatptr1->id;
                fatptr1->id = FREE;
                fatptr2->id = FREE;
            }
            openfile[fd].count = 0;
            openfile[fd].length = 0;
            break;
        case 2:
            openfile[fd].count = 0;
            //文件的读写指针应该不需要变
            break;
        case 3:
            openfile[fd].count = openfile[fd].length;
        default:
            break;
    }
    printf("Please input text that you want to write:\n");
    while(gets(text))
    {
        len = strlen(text);
        num_cur = do_write(fd,text,len,wstyle);
        if(num_cur != ERROR)
            num += num_cur;
        if(num_cur < len)
        {
            printf("Write ERROR!\n");
            break;
        }
    }
    //printf("In my_write, the fd is %d, the filename is %s,the length is %d\n",fd, openfile[fd].filename,openfile[fd].length);
    return num;
}

//实际写文件函数
int do_write(int fd,char *text,int len, char wstyle)
{
    fat *fat1,*fat2,*fatptr1,*fatptr2;
    unsigned short blk_num;
    int blk_off,i,num,cur_blk,father;
    unsigned char *pos;
    fcb *fcbptr;
    char text2[MAXTEXT];

    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);
    //逻辑块块号
    blk_num = openfile[fd].count / BLOCKSIZE;
    //块内偏移量
    blk_off = openfile[fd].count;
    fatptr1 = fat1 + openfile[fd].first;
    fatptr2 = fat2 + openfile[fd].first;
    cur_blk = openfile[fd].first;
     //根据fat表找到要写入的起始逻辑块的位置
    for(i = 0;i < blk_num; i++)
    {
        cur_blk = fatptr1->id;
        blk_off = blk_off - BLOCKSIZE;
        fatptr1 = fat1 + cur_blk;
    }
    //写的同时还要考虑写到哪里
    num = 0;
    while(num < len)
    {
        pos = (unsigned char *)(myvhard + cur_blk * BLOCKSIZE);
        //读要整块一起读，写也要整块一起写
        for( i = 0; i < BLOCKSIZE; i++)
            buf[i] = pos[i];
        for(;blk_off < BLOCKSIZE; blk_off++)
        {
            buf[blk_off] = text[num];
            num++;
            openfile[fd].count++;
            //这是写入长度小于（磁盘块大小 - 初始块内偏移量）的时候。
            if(num == len)
                break;
        }
        for( i = 0; i< BLOCKSIZE; i++)
        {
            pos[i] = buf[i];
        }
        //如果写入长度大于一个(磁盘块大小 - 初始块内偏移量)的时候
        if(num < len)
        {
            cur_blk = fatptr1->id;
            if(cur_blk == END)
            {
                cur_blk = findFreeBlock();
                if(cur_blk == ERROR)
                    break;
                fatptr1->id = cur_blk;
                fatptr2->id = cur_blk;
                fatptr1 = fatptr1 + cur_blk;
                fatptr2 = fatptr2 + cur_blk;
                fatptr1->id = END;
                fatptr2->id = END;
            }
            else
            {
                fatptr1 = fat1 + cur_blk;
                fatptr1 = fat1 + cur_blk;
            }
            blk_off = 0;
        }
    }
    if(openfile[fd].count > openfile[fd].length)
    {
        openfile[fd].length = openfile[fd].count;
    }

    //文件的fcb是否被修改
    openfile[fd].fcbstate = 1;
    return num;
}

//读文件函数
int my_read(int fd,int len)
{
    int num;
    char text[MAXTEXT];

    if(fd < 0 || fd >= MAXOPENFILE)
    {
        printf("the input fd is out of range!\n");
        return ERROR;
    }
    openfile[fd].count = 0;
    num = do_read(fd,len,text);
    if(num == ERROR)
    {
        printf("Read fails!\n");
        return ERROR;
    }
    else
    {
        printf("%s\n",text);
    }
    return num;
}

//实际读文件函数
int do_read(int fd, int len, char *text)
{
    fat* fat1;
    fat* fat_ptr;
    int cur_blk,i;
    unsigned short blk_num;
    unsigned char* pos;
    int blk_off,num = 0; // num记录读取长度

    //判断读取长度是否超过文件长度
    if((openfile[fd].count + len) > openfile[fd].length)
    {
        printf("the read length is more than the file's length!\n");
        return ERROR;
    }

    //逻辑块块号
    blk_num = openfile[fd].count / BLOCKSIZE;
    //块内偏移量
    blk_off = openfile[fd].count;
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat_ptr = fat1 + openfile[fd].first;
    cur_blk = openfile[fd].first;
    //根据fat表找到要读取的起始逻辑块的位置
    for(i = 0;i < blk_num;i++)
    {
        cur_blk = fat_ptr->id;
        blk_off = blk_off - BLOCKSIZE;
        fat_ptr = fat1 + cur_blk;
    }
    while(num < len)
    {
        //pos标记当前读写磁盘块的初始位置
        pos = (unsigned char*)(myvhard + cur_blk * BLOCKSIZE);
        for(i = 0;i < BLOCKSIZE;i ++)
        {
            buf[i] = pos[i];
        }
        for(;blk_off < BLOCKSIZE; blk_off++)
        {
            text[num] = buf[blk_off];
            num++;
            openfile[fd].count++;
            //要判断是不是已经读到指定长度了
            if(num == len)
                break;
        }
        if(num < len)
        {
            cur_blk = fat_ptr->id;
            blk_off = 0;
            fat_ptr = fat1 + cur_blk;
        }
    }
    text[num] = '\0';
    return num;
}

//退出文件系统函数
void my_exitsys()
{
    int father;
    FILE *fp;

    fp = fopen(filesys_name,"w");
    while(curdir)
    {
        father = openfile[curdir].father;
        my_close(curdir);
        curdir = father;
    }
    //将磁盘上内容写入磁盘指定文件中
    fwrite(myvhard, SIZE, 1, fp);
    fclose(fp);
    free(buf);
    free(myvhard);
}

//从用户输入中提取出路径
void extractDir(char *dirname, char * dir,char *filename)
{
    int len;
    char *tmp,*next;
    tmp = strtok(dirname,"\\");
    while(tmp)
    {
        if(!(next = strtok(NULL,"\\")))
        {
            if(((strcmp(tmp,".") == 0) || (strcmp(tmp,"..") == 0)))
            {
                break;
            }
            strcpy(filename,tmp);
            break;
        }
        strcat(dir,tmp);
        strcat(dir,"\\");
        tmp = next;
    }
    len = strlen(dir);
    if(dir[len - 1] == '\\')
    {
        dir[len - 1] = '\0';
    }
}

//查找空闲块
int findFreeBlock()
{
    unsigned char i;
    fat *fat1,*pos;
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    for(i = 7;i < (SIZE / BLOCKSIZE); i++)
    {
        pos = fat1 + i;
        if(pos->id == FREE)
            return i;
    }
    printf("Can't find a free block");
    return ERROR;
}

void printOpenFile()
{
    int i,father;
    printf("fd\t\t文件名\t\t文件长度\t\t父目录\t\t父目录fd\n");
    for(i = 0;i < MAXOPENFILE; i++)
    {
        if(openfile[i].topenfile != 0)
        {
            father = openfile[i].father;
            printf("%d\t\t%s.%s\t\t%d\t\t%s\t\t%d\n",i,openfile[i].filename,openfile[i].exname,openfile[i].length,openfile[father].filename,father);
        }

    }
}

void printFcb(int fd)
{
    int i,num,tmp;
    fcb* fcbptr;
    char text[MAXTEXT];

    tmp = openfile[fd].count;
    openfile[fd].count = 0;
    num = do_read(fd, openfile[fd].length, text);
    printf("从目录：%s 中读取了 %d 字节.\n",openfile[fd].filename,num);
    fcbptr = (fcb*)text;
    //定位到文件对应的目录项
    printf("文件名\t\t文件长度\t\t\n");
    for(i = 0; i < (num / sizeof(fcb)); i ++)
    {
        printf("%s\t\t%d\n",fcbptr->filename,fcbptr->length);
        fcbptr++;
    }
    openfile[fd].count = tmp;
}

int main()
{
    char cmd[15][10] = {"cd", "mkdir", "rmdir", "ls", "create", "rm", "open", "close", "write", "read", "format","exit","rm-r","popen"};
    char s[30], *sp;
	int cmdn, flag = 1, i, father;
	block0 *guide;

    //初始化全局变量
    //申请虚拟磁盘空间
    myvhard = (unsigned char *)calloc(SIZE,1);
    //初始化用户打开表
    for(i = 0;i < MAXOPENFILE;i ++)
    {
        openfile[i].topenfile = 0;
    }
    //将根目录设置为当前目录
    strcpy(currentdir,"\\root\\");
    //初始化文件描述符，文件描述符就是一个用户打开表的索引值，指向当前打开的文件
    curdir = 0;
    //初始化虚拟磁盘上数据区开始位置
    guide = (block0*)myvhard;
    startp = guide->startblock;
    //缓冲区
    buf = (unsigned char*)malloc(BLOCKSIZE);
    if(!buf)
    {
        printf("malloc failed!\n");
        return ERROR;
    }
    startsys();
    //printf("In main,openfile[0].length is%d\n",openfile[0].length);
    printf("*********************欢迎来到FAT文件系统*****************************\n\n");
    printf("命令名\t\t命令参数\t\t命令说明\n\n");
    printf("cd\t\t目录名(路径名)\t\t切换当前目录到指定目录\n");
    printf("mkdir\t\t目录名\t\t\t在当前目录创建新目录\n");
    printf("rmdir\t\t目录名\t\t\t在当前目录删除指定目录\n");
    printf("ls\t\t无\t\t\t显示当前目录下的目录和文件\n");
    printf("create\t\t文件名\t\t\t在当前目录下创建指定文件\n");
    printf("rm\t\t文件名\t\t\t在当前目录下删除指定文件\n");
    printf("open\t\t文件名\t\t\t在当前目录下打开指定文件\n");
    printf("write\t\t无\t\t\t在打开文件状态下，写该文件\n");
    printf("read\t\t无\t\t\t在打开文件状态下，读取该文件\n");
    printf("close\t\t无\t\t\t在打开文件状态下，读取该文件\n");
    printf("format\t\t无\t\t\t格式化\n");
    printf("exit\t\t无\t\t\t退出系统\n\n");
    printf("*********************************************************************\n\n");
    while(flag)
    {
        printf("%s>", openfile[curdir].dir);
        gets(s);
        cmdn = -1;
        if(strcmp(s, ""))//如果不为空，执行下面
        {
            sp=strtok(s, " ");
            for(i = 0; i < 15; i++)
            {
                if(strcmp(sp, cmd[i]) == 0)
                {
                    cmdn = i;
                    break;
                }
            }
            switch(cmdn){
                case 0:
                    sp = strtok(NULL, " ");
                    if(sp && (!openfile[curdir].attribute))
                        my_cd(sp);
                    else
                        printf("Please input the right command.\n");
                    break;
                case 1:
                    sp = strtok(NULL, " ");
                    if(sp && (!openfile[curdir].attribute))
                        my_mkdir(sp);
                    else
                        printf("Please input the right command.\n");
                    break;
                case 2:
                    sp = strtok(NULL, " ");
                    if(sp && (!openfile[curdir].attribute))
                        my_rmdir(sp);
                    else
                        printf("Please input the right command.\n");
                    break;
                case 3:
                    if(!openfile[curdir].attribute)
                        my_ls();
                    else
                        printf("Please input the right command.\n");
                    break;
                case 4:
                    sp = strtok(NULL, " ");
                    if(sp && (!openfile[curdir].attribute))
                        my_create(sp);
                    else
                        printf("Please input the right command.\n");
                    break;
                case 5:
                    sp = strtok(NULL, " ");
                    if(sp && (!openfile[curdir].attribute))
                        my_rm(sp);
                    else
                        printf("Please input the right command.\n");
                    break;
                case 6:
                    sp = strtok(NULL, " ");
                    if(sp && (!openfile[curdir].attribute))
                    {
                        if(strchr(sp, '.'))//查找sp中'.'首次出现的位置
                            curdir = my_open(sp);
                        else
                            printf("the openfile should have exname.\n");
                    }
                    else
                        printf("Please input the right command.\n");
                    break;
                case 7:
                    if(!(openfile[curdir].attribute == 0))
                    {
                        father = openfile[curdir].father;
                        my_close(curdir);
                        curdir = father;
                    }
                    else
                        printf("No files opened.\n");
                    break;
                case 8:
                    if(!(openfile[curdir].attribute == 0))
                        my_write(curdir);
                    else
                        printf("No files opened.\n");
                    break;
                case 9:
                    if(!(openfile[curdir].attribute == 0))
                        my_read(curdir, openfile[curdir].length);
                    else
                        printf("No files opened.\n");
                    break;
                case 10:
                     my_format();
                     startsys();
                     break;
                case 11:
                    if(openfile[curdir].attribute == 0)
                    {
                        my_exitsys();
                        flag = 0;
                    }
                    else
                        printf("Please input the right command.\n");
                    break;
                case 12:
                    sp = strtok(NULL, " ");
                    if(sp && (openfile[curdir].attribute == 0))
                        my_rmdir(sp);
                    else
                        printf("Please input the right command.\n");
                    break;
                case 13:
                    printOpenFile();
                    break;
                //case 14:
                   // printFcb();
                    //break;
                default:
                    printf("Please input the right command.\n");
                    break;
            }
        }
    }
    return 0;
}
