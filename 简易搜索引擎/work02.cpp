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

// ===================== 版本4 新增：检索效率对比测试 =====================
void testSearchPerformance() {
    // 先重建索引
    rebuildIndex();
    
    // 构造测试关键词
    char keys[MAX_KEY_NUM][MAX_KEY_LEN] = {"a", "b", "c"};
    int keyNum = 3;
    
    clock_t start, end;
    long t1, t2;
    
    printf("\n===== 线性检索 VS 倒排索引检索 效率对比 =====\n");
    printf("测试文档数：%d\n", DocLibrary.count);
    printf("测试关键词数：%d\n", keyNum);
    
    // 测试线性检索（版本3方式）
    start = clock();
    for (int i = 0; i < 1000; i++) {
        int cnt = 0;
        for (int d = 0; d < DocLibrary.count; d++) {
            int match = 1;
            for (int k = 0; k < keyNum; k++) {
                if (!KMP(DocLibrary.docs[d].title, keys[k]) && !KMP(DocLibrary.docs[d].content, keys[k])) {
                    match = 0;
                    break;
                }
            }
            if (match) cnt++;
        }
    }
    end = clock();
    t1 = end - start;
    printf("线性检索耗时：%ld\n", t1);
    
    // 测试倒排索引检索
    start = clock();
    for (int i = 0; i < 1000; i++) {
        int result[MAX_DOCS];
        int cnt = indexSearchAnd(keys, keyNum, result, MAX_DOCS);
    }
    end = clock();
    t2 = end - start;
    printf("倒排索引检索耗时：%ld\n", t2);
    
    if (t2 < t1) {
        double speedup = (double)t1 / t2;
        printf("? 结论：倒排索引速度提升 %.1f 倍！\n", speedup);
    } else {
        printf("?  结论：文档量过少时，索引优势不明显\n");
    }
}

// ------------------------------
// 原有文档管理功能（已适配索引更新）
// ------------------------------
void initLibrary() { DocLibrary.count = 0; }

int findDocById(const char* id) {
    for (int i = 0; i < DocLibrary.count; i++)
        if (!myStrcmp(DocLibrary.docs[i].id, id)) return i;
    return -1;
}

void addDoc() {
    if (DocLibrary.count >= MAX_DOCS) { printf("已满\n"); return; }
    Document d;
    printf("ID："); scanf("%s", d.id); getchar();
    if (findDocById(d.id) != -1) { printf("ID重复\n"); return; }
    printf("标题："); fgets(d.title, MAX_TITLE_LEN, stdin);
    printf("内容："); fgets(d.content, MAX_CONTENT_LEN, stdin);
    DocLibrary.docs[DocLibrary.count] = d;
    // 为新文档建立索引
    indexDocument(DocLibrary.count);
    DocLibrary.count++;
    printf("添加成功\n");
}

void showAllDocs() {
    if (!DocLibrary.count) { printf("空库\n"); return; }
    for (int i = 0; i < DocLibrary.count; i++)
        printf("ID:%s 标题:%s", DocLibrary.docs[i].id, DocLibrary.docs[i].title);
}

void modifyDoc() {
    char id[MAX_ID_LEN]; printf("ID："); scanf("%s", id);
    int idx = findDocById(id);
    if (idx == -1) { printf("不存在\n"); return; }
    getchar(); 
    // 先从索引中删除旧文档
    removeDocFromIndex(idx);
    printf("新标题："); fgets(DocLibrary.docs[idx].title, MAX_TITLE_LEN, stdin);
    printf("新内容："); fgets(DocLibrary.docs[idx].content, MAX_CONTENT_LEN, stdin);
    // 重新索引修改后的文档
    indexDocument(idx);
    printf("修改成功\n");
}

void deleteDoc() {
    char id[MAX_ID_LEN]; printf("ID："); scanf("%s", id);
    int idx = findDocById(id);
    if (idx == -1) { printf("不存在\n"); return; }
    // 先从索引中删除
    removeDocFromIndex(idx);
    // 删除文档
    for (int i = idx; i < DocLibrary.count - 1; i++) {
        DocLibrary.docs[i] = DocLibrary.docs[i + 1];
    }
    DocLibrary.count--;
    // 重建索引（因为文档下标发生了变化）
    rebuildIndex();
    printf("删除成功\n");
}

void saveDocsToFile() {
    FILE* f = fopen(FILE_NAME, "w");
    if (!f) { printf("失败\n"); return; }
    fprintf(f, "%d\n", DocLibrary.count);
    for (int i = 0; i < DocLibrary.count; i++)
        fprintf(f, "%s\n%s%s", DocLibrary.docs[i].id, DocLibrary.docs[i].title, DocLibrary.docs[i].content);
    fclose(f); printf("保存成功\n");
}

void loadDocsFromFile() {
    FILE* f = fopen(FILE_NAME, "r");
    if (!f) { printf("新建库\n"); return; }
    fscanf(f, "%d", &DocLibrary.count); getchar();
    for (int i = 0; i < DocLibrary.count; i++) {
        fgets(DocLibrary.docs[i].id, MAX_ID_LEN, f);
        DocLibrary.docs[i].id[myStrlen(DocLibrary.docs[i].id) - 1] = 0;
        fgets(DocLibrary.docs[i].title, MAX_TITLE_LEN, f);
        fgets(DocLibrary.docs[i].content, MAX_CONTENT_LEN, f);
    }
    fclose(f); 
    // 加载完成后自动重建索引
    rebuildIndex();
    printf("加载成功\n");
}

// 版本3原有线性检索
void linearSearch() {
    char input[500], keys[MAX_KEY_NUM][MAX_KEY_LEN];
    printf("\n===== 线性检索 =====\n输入关键词（空格分隔）：");
    getchar(); fgets(input, 500, stdin);
    int n = splitKeys(input, keys);
    if (!n) { printf("无关键词\n"); return; }

    int mode;
    printf("1-全部包含 2-任意包含："); scanf("%d", &mode);

    int cnt = 0;
    printf("\n===== 检索结果 =====\n");
    for (int i = 0; i < DocLibrary.count; i++) {
        int ok = 1;
        if (mode == 1) {
            for (int k = 0; k < n; k++) {
                if (!KMP(DocLibrary.docs[i].title, keys[k]) && !KMP(DocLibrary.docs[i].content, keys[k])) {
                    ok = 0;
                    break;
                }
            }
        } else {
            ok = 0;
            for (int k = 0; k < n; k++) {
                if (KMP(DocLibrary.docs[i].title, keys[k]) || KMP(DocLibrary.docs[i].content, keys[k])) {
                    ok = 1;
                    break;
                }
            }
        }
        if (ok) {
            cnt++;
            printf("匹配%d ID:%s 标题:%s", cnt, DocLibrary.docs[i].id, DocLibrary.docs[i].title);
        }
    }
    printf("共匹配：%d\n", cnt);
}

// BF/KMP算法对比测试
void testAlgorithmPerformance() {
    char text[MAX_CONTENT_LEN];
    char pattern[] = "abcde";

    for (int i = 0; i < MAX_CONTENT_LEN - 10; i++)
        text[i] = 'a' + i % 26;
    text[MAX_CONTENT_LEN - 1] = '\0';

    text[MAX_CONTENT_LEN - 6] = 'a';
    text[MAX_CONTENT_LEN - 5] = 'b';
    text[MAX_CONTENT_LEN - 4] = 'c';
    text[MAX_CONTENT_LEN - 3] = 'd';
    text[MAX_CONTENT_LEN - 2] = 'e';

    clock_t start, end;
    long t1, t2;

    printf("\n===== BF VS KMP 算法效率测试 =====\n");
    printf("测试文本长度：%d\n", myStrlen(text));
    printf("测试关键词长度：%d\n", myStrlen(pattern));

    start = clock();
    for (int i = 0; i < 10000; i++) {
        // BF算法
        int n = myStrlen(text), m = myStrlen(pattern);
        int found = 0;
        for (int i = 0; i <= n - m; i++) {
            int j;
            for (j = 0; j < m; j++)
                if (text[i + j] != pattern[j]) break;
            if (j == m) { found = 1; break; }
        }
    }
    end = clock();
    t1 = end - start;
    printf("BF 算法耗时：%ld\n", t1);

    start = clock();
    for (int i = 0; i < 10000; i++) KMP(text, pattern);
    end = clock();
    t2 = end - start;
    printf("KMP 算法耗时：%ld\n", t2);

    if (t2 < t1)
        printf("? 结论：KMP 效率明显更高！\n");
    else
        printf("?  结论：短文本差距小\n");
}

// ------------------------------
// 菜单（版本4新增选项）
// ------------------------------
void showMenu() {
    printf("\n===== 简易搜索引擎 版本4：倒排索引 =====\n");
    printf("1.新增文档  2.查看所有  3.修改文档  4.删除文档\n");
    printf("5.保存文件  6.加载文件  7.线性检索  8.倒排索引检索\n");
    printf("9.重建索引  10.BF/KMP对比  11.检索效率对比\n");
    printf("0.退出\n选项：");
}

int main() {
    initLibrary();
    initIndex();
    loadDocsFromFile();
    int op;
    while (1) {
        showMenu();
        scanf("%d", &op);
        switch (op) {
            case 1: addDoc(); break;
            case 2: showAllDocs(); break;
            case 3: modifyDoc(); break;
            case 4: deleteDoc(); break;
            case 5: saveDocsToFile(); break;
            case 6: loadDocsFromFile(); break;
            case 7: linearSearch(); break;
            case 8: indexSearch(); break;
            case 9: rebuildIndex(); break;
            case 10: testAlgorithmPerformance(); break;
            case 11: testSearchPerformance(); break;
            case 0: return 0;
            default: printf("错误\n");
        }
    }
}
