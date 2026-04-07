#include <stdio.h>
#include <stdlib.h>

// ===================== 配置常量 =====================
#define MAX_DOCS 100       // 最大文档数量
#define MAX_ID_LEN 20      // 文档ID最大长度
#define MAX_TITLE_LEN 100  // 标题最大长度
#define MAX_CONTENT_LEN 1000 // 内容最大长度
#define FILE_NAME "docs.txt" // 数据存储文件

// ===================== 核心数据结构 =====================
// 文档结构体（版本1关键数据结构）
typedef struct {
    char id[MAX_ID_LEN];       // 文档唯一标识
    char title[MAX_TITLE_LEN]; // 文档标题
    char content[MAX_CONTENT_LEN]; // 文档内容
} Document;

// 文档库：存储所有文档 + 当前文档数量
struct {
    Document docs[MAX_DOCS];
    int count; // 当前有效文档数
} DocLibrary;

// ===================== 自定义工具函数（无STL） =====================
// 计算字符串长度
int myStrlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

// 字符串复制
void myStrcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// 字符串比较（相等返回0，不等返回非0）
int myStrcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return a[i] - b[i];
        i++;
    }
    return myStrlen(a) - myStrlen(b);
}

// ===================== 文档库核心功能 =====================
// 1. 初始化文档库
void initLibrary() {
    DocLibrary.count = 0;
}

// 2. 根据ID查找文档，返回下标，找不到返回-1
int findDocById(const char* id) {
    for (int i = 0; i < DocLibrary.count; i++) {
        if (myStrcmp(DocLibrary.docs[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

// 3. 添加文档
void addDoc() {
    if (DocLibrary.count >= MAX_DOCS) {
        printf("? 文档库已满，无法添加！\n");
        return;
    }

    Document newDoc;
    printf("===== 新增文档 =====\n");
    printf("请输入文档ID：");
    scanf("%s", newDoc.id);
    getchar(); // 吸收回车

    // 判断ID是否重复
    if (findDocById(newDoc.id) != -1) {
        printf("? 文档ID已存在！\n");
        return;
    }

    printf("请输入文档标题：");
    fgets(newDoc.title, MAX_TITLE_LEN, stdin);
    printf("请输入文档内容：");
    fgets(newDoc.content, MAX_CONTENT_LEN, stdin);

    // 存入文档库
    DocLibrary.docs[DocLibrary.count] = newDoc;
    DocLibrary.count++;
    printf("? 文档添加成功！\n");
}

// 4. 查看所有文档
void showAllDocs() {
    if (DocLibrary.count == 0) {
        printf("?? 文档库为空！\n");
        return;
    }

    printf("\n===== 所有文档（共%d篇）=====\n", DocLibrary.count);
    for (int i = 0; i < DocLibrary.count; i++) {
        printf("第%d篇\n", i + 1);
        printf("ID：%s\n", DocLibrary.docs[i].id);
        printf("标题：%s", DocLibrary.docs[i].title);
        printf("内容：%s", DocLibrary.docs[i].content);
        printf("------------------------\n");
    }
}

// 5. 修改文档
void modifyDoc() {
    char id[MAX_ID_LEN];
    printf("请输入要修改的文档ID：");
    scanf("%s", id);
    int idx = findDocById(id);

    if (idx == -1) {
        printf("? 未找到该文档！\n");
        return;
    }

    getchar();
    printf("===== 修改文档 =====\n");
    printf("请输入新标题：");
    fgets(DocLibrary.docs[idx].title, MAX_TITLE_LEN, stdin);
    printf("请输入新内容：");
    fgets(DocLibrary.docs[idx].content, MAX_CONTENT_LEN, stdin);
    printf("? 修改成功！\n");
}

// 6. 删除文档
void deleteDoc() {
    char id[MAX_ID_LEN];
    printf("请输入要删除的文档ID：");
    scanf("%s", id);
    int idx = findDocById(id);

    if (idx == -1) {
        printf("? 未找到该文档！\n");
        return;
    }

    // 后面的文档向前覆盖
    for (int i = idx; i < DocLibrary.count - 1; i++) {
        DocLibrary.docs[i] = DocLibrary.docs[i + 1];
    }
    DocLibrary.count--;
    printf("? 删除成功！\n");
}

// 7. 保存文档到文件
void saveDocsToFile() {
    FILE* fp = fopen(FILE_NAME, "w");
    if (!fp) {
        printf("? 文件打开失败！\n");
        return;
    }

    // 写入数量 + 每篇文档
    fprintf(fp, "%d\n", DocLibrary.count);
    for (int i = 0; i < DocLibrary.count; i++) {
        fprintf(fp, "%s\n", DocLibrary.docs[i].id);
        fprintf(fp, "%s", DocLibrary.docs[i].title);
        fprintf(fp, "%s", DocLibrary.docs[i].content);
    }

    fclose(fp);
    printf("? 已保存到 %s\n", FILE_NAME);
}

// 8. 从文件加载文档
void loadDocsFromFile() {
    FILE* fp = fopen(FILE_NAME, "r");
    if (!fp) {
        printf("?? 未找到数据文件，创建新库\n");
        return;
    }

    // 读取数量
    fscanf(fp, "%d", &DocLibrary.count);
    getchar();

    for (int i = 0; i < DocLibrary.count; i++) {
        fgets(DocLibrary.docs[i].id, MAX_ID_LEN, fp);
        // 去除换行
        DocLibrary.docs[i].id[myStrlen(DocLibrary.docs[i].id) - 1] = '\0';

        fgets(DocLibrary.docs[i].title, MAX_TITLE_LEN, fp);
        fgets(DocLibrary.docs[i].content, MAX_CONTENT_LEN, fp);
    }

    fclose(fp);
    printf("? 从文件加载成功！\n");
}

// ===================== 菜单 =====================
void showMenu() {
    printf("\n========== 简易搜索引擎 - 版本1 ==========\n");
    printf("1. 新增文档\n");
    printf("2. 查看所有文档\n");
    printf("3. 修改文档\n");
    printf("4. 删除文档\n");
    printf("5. 保存文档到文件\n");
    printf("6. 从文件加载文档\n");
    printf("0. 退出程序\n");
    printf("=========================================\n");
    printf("请输入操作序号：");
}

// ===================== 主函数 =====================
int main() {
    initLibrary();
    loadDocsFromFile(); // 启动自动加载

    int choice;
    while (1) {
        showMenu();
        scanf("%d", &choice);

        switch (choice) {
            case 1: addDoc(); break;
            case 2: showAllDocs(); break;
            case 3: modifyDoc(); break;
            case 4: deleteDoc(); break;
            case 5: saveDocsToFile(); break;
            case 6: loadDocsFromFile(); break;
            case 0:
                printf("?? 程序退出\n");
                return 0;
            default:
                printf("? 输入错误，请重试！\n");
        }
    }
    return 0;
}
