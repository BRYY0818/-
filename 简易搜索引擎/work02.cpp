#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ===================== 配置常量 =====================
#define MAX_DOCS 100       // 最大文档数量
#define MAX_ID_LEN 20      // 文档ID最大长度
#define MAX_TITLE_LEN 100  // 标题最大长度
#define MAX_CONTENT_LEN 2000 // 内容最大长度
#define MAX_KEY_LEN 50     // 关键词最大长度
#define MAX_KEY_NUM 10     // 最多支持10个关键词
#define MAX_INDEX_ITEMS 500  // 倒排索引最大词条数
#define MAX_DOCS_PER_KEY 50  // 每个关键词最多关联文档数
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

// ===================== 版本4 新增：倒排索引数据结构 =====================
// 倒排索引项：关键词 + 关联的文档ID列表
typedef struct {
    char keyword[MAX_KEY_LEN];
    int docIds[MAX_DOCS_PER_KEY];  // 存储文档在数组中的下标
    int docCount;                  // 关联的文档数量
} IndexItem;

// 倒排索引表
struct {
    IndexItem items[MAX_INDEX_ITEMS];
    int count;  // 当前词条数量
} InvertedIndex;

// ------------------------------
// 工具函数
// ------------------------------
int myStrlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

void myStrcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

int myStrcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] - b[i];
}

// 按空格分割字符串为关键词
int splitKeys(char* in, char k[MAX_KEY_NUM][MAX_KEY_LEN]) {
    int c = 0, idx = 0, len = myStrlen(in);
    for (int i = 0; i < len && c < MAX_KEY_NUM; i++) {
        if (in[i] == ' ' || in[i] == '\n' || in[i] == '\t') {
            if (idx) { k[c][idx] = 0; c++; idx = 0; }
        } else k[c][idx++] = in[i];
    }
    if (idx && c < MAX_KEY_NUM) { k[c][idx] = 0; c++; }
    return c;
}

// KMP算法（默认字符串匹配）
void getNext(const char* pat, int* next) {
    int m = myStrlen(pat);
    next[0] = -1;
    int i = 0, j = -1;
    while (i < m) {
        if (j == -1 || pat[i] == pat[j]) {
            i++; j++; next[i] = j;
        } else j = next[j];
    }
}

int KMP(const char* text, const char* pat) {
    int n = myStrlen(text);
    int m = myStrlen(pat);
    if (m == 0 || m > n) return 0;

    int next[100];
    getNext(pat, next);

    int i = 0, j = 0;
    while (i < n && j < m) {
        if (j == -1 || text[i] == pat[j]) {
            i++; j++;
        } else j = next[j];
    }
    return (j == m) ? 1 : 0;
}

// ===================== 版本4 新增：倒排索引核心函数 =====================
// 初始化倒排索引
void initIndex() {
    InvertedIndex.count = 0;
}

// 查找关键词在索引中的位置，不存在返回-1
int findIndexItem(const char* keyword) {
    for (int i = 0; i < InvertedIndex.count; i++) {
        if (myStrcmp(InvertedIndex.items[i].keyword, keyword) == 0) {
            return i;
        }
    }
    return -1;
}

// 向索引中添加一个关键词-文档映射
void addToIndex(const char* keyword, int docIdx) {
    int idx = findIndexItem(keyword);
    if (idx == -1) {
        // 关键词不存在，新建索引项
        if (InvertedIndex.count >= MAX_INDEX_ITEMS) return;
        myStrcpy(InvertedIndex.items[InvertedIndex.count].keyword, keyword);
        InvertedIndex.items[InvertedIndex.count].docIds[0] = docIdx;
        InvertedIndex.items[InvertedIndex.count].docCount = 1;
        InvertedIndex.count++;
    } else {
        // 关键词已存在，检查文档是否已关联
        IndexItem* item = &InvertedIndex.items[idx];
        for (int i = 0; i < item->docCount; i++) {
            if (item->docIds[i] == docIdx) return;
        }
        // 添加新的文档关联
        if (item->docCount < MAX_DOCS_PER_KEY) {
            item->docIds[item->docCount] = docIdx;
            item->docCount++;
        }
    }
}

// 为单篇文档建立索引
void indexDocument(int docIdx) {
    Document* doc = &DocLibrary.docs[docIdx];
    char keys[MAX_KEY_NUM][MAX_KEY_LEN];
    
    // 索引标题
    char titleCopy[MAX_TITLE_LEN];
    myStrcpy(titleCopy, doc->title);
    int titleKeyNum = splitKeys(titleCopy, keys);
    for (int i = 0; i < titleKeyNum; i++) {
        addToIndex(keys[i], docIdx);
    }
    
    // 索引内容
    char contentCopy[MAX_CONTENT_LEN];
    myStrcpy(contentCopy, doc->content);
    int contentKeyNum = splitKeys(contentCopy, keys);
    for (int i = 0; i < contentKeyNum; i++) {
        addToIndex(keys[i], docIdx);
    }
}

// 重建整个倒排索引
void rebuildIndex() {
    initIndex();
    for (int i = 0; i < DocLibrary.count; i++) {
        indexDocument(i);
    }
    printf("? 倒排索引重建完成，共%d个词条\n", InvertedIndex.count);
}

// 从索引中删除文档
void removeDocFromIndex(int docIdx) {
    for (int i = 0; i < InvertedIndex.count; i++) {
        IndexItem* item = &InvertedIndex.items[i];
        int j;
        for (j = 0; j < item->docCount; j++) {
            if (item->docIds[j] == docIdx) break;
        }
        if (j < item->docCount) {
            // 前移覆盖
            for (int k = j; k < item->docCount - 1; k++) {
                item->docIds[k] = item->docIds[k + 1];
            }
            item->docCount--;
        }
    }
}

// ===================== 版本4 新增：基于倒排索引的检索 =====================
// 获取单个关键词的匹配文档列表
int getDocsByKeyword(const char* keyword, int result[], int maxResult) {
    int idx = findIndexItem(keyword);
    if (idx == -1) return 0;
    
    IndexItem* item = &InvertedIndex.items[idx];
    int count = (item->docCount < maxResult) ? item->docCount : maxResult;
    for (int i = 0; i < count; i++) {
        result[i] = item->docIds[i];
    }
    return count;
}

// 多关键词与检索（交集）
int indexSearchAnd(char keys[MAX_KEY_NUM][MAX_KEY_LEN], int keyNum, int result[], int maxResult) {
    if (keyNum == 0) return 0;
    
    int temp[MAX_DOCS];
    int count = getDocsByKeyword(keys[0], temp, MAX_DOCS);
    
    for (int i = 1; i < keyNum; i++) {
        int current[MAX_DOCS];
        int currentCount = getDocsByKeyword(keys[i], current, MAX_DOCS);
        
        // 求交集
        int newCount = 0;
        for (int j = 0; j < count; j++) {
            for (int k = 0; k < currentCount; k++) {
                if (temp[j] == current[k]) {
                    temp[newCount++] = temp[j];
                    break;
                }
            }
        }
        count = newCount;
        if (count == 0) break;
    }
    
    // 复制结果
    int finalCount = (count < maxResult) ? count : maxResult;
    for (int i = 0; i < finalCount; i++) {
        result[i] = temp[i];
    }
    return finalCount;
}

// 多关键词或检索（并集）
int indexSearchOr(char keys[MAX_KEY_NUM][MAX_KEY_LEN], int keyNum, int result[], int maxResult) {
    int temp[MAX_DOCS] = {0};
    int count = 0;
    
    for (int i = 0; i < keyNum; i++) {
        int current[MAX_DOCS];
        int currentCount = getDocsByKeyword(keys[i], current, MAX_DOCS);
        
        // 求并集
        for (int j = 0; j < currentCount; j++) {
            int exists = 0;
            for (int k = 0; k < count; k++) {
                if (temp[k] == current[j]) {
                    exists = 1;
                    break;
                }
            }
            if (!exists && count < MAX_DOCS) {
                temp[count++] = current[j];
            }
        }
    }
    
    // 复制结果
    int finalCount = (count < maxResult) ? count : maxResult;
    for (int i = 0; i < finalCount; i++) {
        result[i] = temp[i];
    }
    return finalCount;
}

// 倒排索引检索入口
void indexSearch() {
    char input[500], keys[MAX_KEY_NUM][MAX_KEY_LEN];
    printf("\n===== 倒排索引检索 =====\n输入关键词（空格分隔）：");
    getchar(); fgets(input, 500, stdin);
    int n = splitKeys(input, keys);
    if (!n) { printf("无关键词\n"); return; }

    int mode;
    printf("1-全部包含(AND) 2-任意包含(OR)："); scanf("%d", &mode);

    int result[MAX_DOCS];
    int matchCount;
    
    if (mode == 1) {
        matchCount = indexSearchAnd(keys, n, result, MAX_DOCS);
    } else {
        matchCount = indexSearchOr(keys, n, result, MAX_DOCS);
    }

    printf("\n===== 检索结果 =====\n");
    for (int i = 0; i < matchCount; i++) {
        int docIdx = result[i];
        printf("匹配%d ID:%s 标题:%s", i+1, DocLibrary.docs[docIdx].id, DocLibrary.docs[docIdx].title);
    }
    printf("共匹配：%d\n", matchCount);
}

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

// ===================== 版本2 新增：子串匹配（判断是否包含关键词） =====================
// 功能：在text中查找keyword，找到返回1，找不到返回0
int containsKey(const char* text, const char* keyword) {
    int textLen = myStrlen(text);
    int keyLen = myStrlen(keyword);

    if (keyLen == 0 || keyLen > textLen) return 0;

    // 朴素匹配算法
    for (int i = 0; i <= textLen - keyLen; i++) {
        int match = 1;
        for (int j = 0; j < keyLen; j++) {
            if (text[i + j] != keyword[j]) {
                match = 0;
                break;
            }
        }
        if (match == 1) return 1;
    }
    return 0;
}

// ===================== 版本3 新增：按空格分割多个关键词 =====================
// input：输入整行字符串 keys：存放分割后的关键词数组 返回关键词个数
int splitKeys(char* input, char keys[MAX_KEY_NUM][MAX_KEY_LEN]) {
    int cnt = 0;
    int idx = 0;
    int len = myStrlen(input);

    for (int i = 0; i < len && cnt < MAX_KEY_NUM; i++) {
        if (input[i] == ' ' || input[i] == '\n') {
            if (idx > 0) {
                keys[cnt][idx] = '\0';
                cnt++;
                idx = 0;
            }
        } else {
            keys[cnt][idx++] = input[i];
        }
    }
    // 处理最后一个关键词
    if (idx > 0 && cnt < MAX_KEY_NUM) {
        keys[cnt][idx] = '\0';
        cnt++;
    }
    return cnt;
}

// 判断一篇文档是否匹配所有关键词（AND）
int matchAllKeys(Document doc, char keys[MAX_KEY_NUM][MAX_KEY_LEN], int keyNum) {
    for (int k = 0; k < keyNum; k++) {
        int inTitle = containsKey(doc.title, keys[k]);
        int inCont = containsKey(doc.content, keys[k]);
        if (!inTitle && !inCont) {
            return 0;
        }
    }
    return 1;
}

// 判断一篇文档是否匹配任意关键词（OR）
int matchAnyKey(Document doc, char keys[MAX_KEY_NUM][MAX_KEY_LEN], int keyNum) {
    for (int k = 0; k < keyNum; k++) {
        int inTitle = containsKey(doc.title, keys[k]);
        int inCont = containsKey(doc.content, keys[k]);
        if (inTitle || inCont) {
            return 1;
        }
    }
    return 0;
}

// 多关键词检索入口
void searchMultiKey() {
    char input[500];
    char keys[MAX_KEY_NUM][MAX_KEY_LEN];

    printf("\n===== 多关键词检索 =====\n");
    printf("请输入多个关键词，用空格分隔：\n");
    getchar();
    fgets(input, 500, stdin);

    int keyNum = splitKeys(input, keys);
    if (keyNum <= 0) {
        printf("未输入有效关键词！\n");
        return;
    }

    int mode;
    printf("请选择检索模式：\n");
    printf("1-与检索(包含所有关键词)  2-或检索(包含任意一个)\n");
    scanf("%d", &mode);

    int matchCount = 0;
    printf("\n===== 检索结果 =====\n");

    for (int i = 0; i < DocLibrary.count; i++) {
        int res = 0;
        if (mode == 1) {
            res = matchAllKeys(DocLibrary.docs[i], keys, keyNum);
        } else if (mode == 2) {
            res = matchAnyKey(DocLibrary.docs[i], keys, keyNum);
        }

        if (res) {
            matchCount++;
            printf("匹配文档 %d\n", matchCount);
            printf("ID：%s\n", DocLibrary.docs[i].id);
            printf("标题：%s", DocLibrary.docs[i].title);
            printf("内容：%s", DocLibrary.docs[i].content);
            printf("------------------------\n");
        }
    }

    if (matchCount == 0) {
        printf("未找到匹配文档\n");
    } else {
        printf("共找到 %d 篇匹配文档\n", matchCount);
    }
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

// ===================== 版本2 新增：关键词检索功能 =====================
void searchByKeyword() {
    char key[MAX_KEY_LEN];
    printf("===== 关键词检索 =====\n");
    printf("请输入要搜索的关键词：");
    getchar();
    fgets(key, MAX_KEY_LEN, stdin);

    // 去掉fgets读取的换行符
    int len = myStrlen(key);
    if (len > 0 && key[len - 1] == '\n') {
        key[len - 1] = '\0';
    }

    int matchCount = 0;
    printf("\n===== 检索结果 =====\n");

    for (int i = 0; i < DocLibrary.count; i++) {
        // 同时匹配标题和内容
        int titleMatch = containsKey(DocLibrary.docs[i].title, key);
        int contentMatch = containsKey(DocLibrary.docs[i].content, key);

        if (titleMatch || contentMatch) {
            matchCount++;
            printf("匹配文档 %d\n", matchCount);
            printf("ID：%s\n", DocLibrary.docs[i].id);
            printf("标题：%s", DocLibrary.docs[i].title);
            printf("内容：%s", DocLibrary.docs[i].content);
            printf("------------------------\n");
        }
    }

    if (matchCount == 0) {
        printf("?? 未找到包含关键词「%s」的文档\n", key);
    } else {
        printf("? 共找到 %d 篇匹配文档\n", matchCount);
    }
}

// ===================== 菜单 =====================
void showMenu() {
    printf("\n========== 简易搜索引擎 - 版本2 ==========\n");
    printf("1. 新增文档\n");
    printf("2. 查看所有文档\n");
    printf("3. 修改文档\n");
    printf("4. 删除文档\n");
    printf("5. 保存文档到文件\n");
    printf("6. 从文件加载文档\n");
    printf("7. 关键词检索 （新增）\n")；
    printf("8. 多关键词检索\n");
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
            case 7: searchByKeyword(); break;
            case 8: searchMultiKey();break;
            case 0:
                printf("?? 程序退出\n");
                return 0;
            default:
                printf("? 输入错误，请重试！\n");
        }
    }
    return 0;
}
