#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ===================== 全局常量配置 =====================
#define MAX_DOCS 100
#define MAX_ID_LEN 20
#define MAX_TITLE_LEN 100
#define MAX_CONTENT_LEN 2000
#define MAX_KEY_LEN 50
#define MAX_KEY_NUM 10
#define MAX_INDEX_ITEMS 500
#define MAX_DOCS_PER_KEY 50
#define MAX_RECOMMEND 5
#define SIMILARITY_THRESHOLD 2
#define FILE_NAME "docs.txt"

// ===================== 核心数据结构 =====================
// 文档结构体
typedef struct {
    char id[MAX_ID_LEN];
    char title[MAX_TITLE_LEN];
    char content[MAX_CONTENT_LEN];
} Document;

// 文档库全局变量
struct {
    Document docs[MAX_DOCS];
    int count;
} DocLibrary;

// 倒排索引项
typedef struct {
    char keyword[MAX_KEY_LEN];
    int docIds[MAX_DOCS_PER_KEY];
    int docCount;
} IndexItem;

// 倒排索引全局变量
struct {
    IndexItem items[MAX_INDEX_ITEMS];
    int count;
} InvertedIndex;

// 版本6新增：检索结果结构体（带得分）
typedef struct {
    int docIdx;     // 文档下标
    int score;      // 相关度得分（词频总和）
} SearchResult;

// 图数据结构
int docGraph[MAX_DOCS][MAX_DOCS];
int keywordGraph[MAX_INDEX_ITEMS][MAX_INDEX_ITEMS];
int visited[MAX_DOCS];

// ===================== 自定义字符串工具函数 =====================
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

// 中文/多分隔符分词
int splitChinese(char* in, char k[MAX_KEY_NUM][MAX_KEY_LEN]) {
    int c = 0, idx = 0, len = myStrlen(in);
    char separators[] = " ,.!?，。！？；：;:、\n\t";
    
    for (int i = 0; i < len && c < MAX_KEY_NUM; i++) {
        int isSep = 0;
        for (int j = 0; separators[j] != '\0'; j++) {
            if (in[i] == separators[j]) { isSep = 1; break; }
        }
        if (isSep) {
            if (idx) { k[c][idx] = 0; c++; idx = 0; }
        } else {
            k[c][idx++] = in[i];
        }
    }
    if (idx && c < MAX_KEY_NUM) { k[c][idx] = 0; c++; }
    return c;
}

// ===================== KMP字符串匹配（含词频统计） =====================
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

// 基础匹配：返回1存在，0不存在
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

// 版本6新增：统计关键词出现次数（用于相关度计算）
int countKeywordTimes(const char* text, const char* pat) {
    int n = myStrlen(text);
    int m = myStrlen(pat);
    if (m == 0 || m > n) return 0;

    int next[100];
    getNext(pat, next);

    int i = 0, j = 0, count = 0;
    while (i < n) {
        if (j == -1 || text[i] == pat[j]) {
            i++; j++;
        } else {
            j = next[j];
        }
        if (j == m) {
            count++;
            j = next[j]; // 匹配成功后继续向后匹配
        }
    }
    return count;
}

// ===================== 倒排索引核心函数 =====================
void initIndex() {
    InvertedIndex.count = 0;
}

int findIndexItem(const char* keyword) {
    for (int i = 0; i < InvertedIndex.count; i++) {
        if (myStrcmp(InvertedIndex.items[i].keyword, keyword) == 0) return i;
    }
    return -1;
}

void addToIndex(const char* keyword, int docIdx) {
    int idx = findIndexItem(keyword);
    if (idx == -1) {
        if (InvertedIndex.count >= MAX_INDEX_ITEMS) return;
        myStrcpy(InvertedIndex.items[InvertedIndex.count].keyword, keyword);
        InvertedIndex.items[InvertedIndex.count].docIds[0] = docIdx;
        InvertedIndex.items[InvertedIndex.count].docCount = 1;
        InvertedIndex.count++;
    } else {
        IndexItem* item = &InvertedIndex.items[idx];
        for (int i = 0; i < item->docCount; i++) {
            if (item->docIds[i] == docIdx) return;
        }
        if (item->docCount < MAX_DOCS_PER_KEY) {
            item->docIds[item->docCount] = docIdx;
            item->docCount++;
        }
    }
}

void indexDocument(int docIdx) {
    Document* doc = &DocLibrary.docs[docIdx];
    char keys[MAX_KEY_NUM][MAX_KEY_LEN];
    
    char titleCopy[MAX_TITLE_LEN];
    myStrcpy(titleCopy, doc->title);
    int titleKeyNum = splitChinese(titleCopy, keys);
    for (int i = 0; i < titleKeyNum; i++) addToIndex(keys[i], docIdx);
    
    char contentCopy[MAX_CONTENT_LEN];
    myStrcpy(contentCopy, doc->content);
    int contentKeyNum = splitChinese(contentCopy, keys);
    for (int i = 0; i < contentKeyNum; i++) addToIndex(keys[i], docIdx);
}

void rebuildIndex() {
    initIndex();
    for (int i = 0; i < DocLibrary.count; i++) indexDocument(i);
    printf("[系统] 倒排索引重建完成，共 %d 个词条\n", InvertedIndex.count);
}

void removeDocFromIndex(int docIdx) {
    for (int i = 0; i < InvertedIndex.count; i++) {
        IndexItem* item = &InvertedIndex.items[i];
        int j;
        for (j = 0; j < item->docCount; j++) {
            if (item->docIds[j] == docIdx) break;
        }
        if (j < item->docCount) {
            for (int k = j; k < item->docCount - 1; k++) {
                item->docIds[k] = item->docIds[k + 1];
            }
            item->docCount--;
        }
    }
}

// ===================== 版本6新增：结果排序核心函数 =====================
// 计算单篇文档对多个关键词的总相关度得分
int calcDocScore(int docIdx, char keys[MAX_KEY_NUM][MAX_KEY_LEN], int keyNum) {
    int score = 0;
    Document* doc = &DocLibrary.docs[docIdx];
    for (int i = 0; i < keyNum; i++) {
        // 标题权重 *2，内容权重 *1
        score += countKeywordTimes(doc->title, keys[i]) * 2;
        score += countKeywordTimes(doc->content, keys[i]) * 1;
    }
    return score;
}

// 冒泡排序：按得分降序排列检索结果
void sortResults(SearchResult results[], int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (results[j].score < results[j + 1].score) {
                SearchResult temp = results[j];
                results[j] = results[j + 1];
                results[j + 1] = temp;
            }
        }
    }
}

// ===================== 倒排索引检索（带排序） =====================
int getDocsByKeyword(const char* keyword, int result[], int maxResult) {
    int idx = findIndexItem(keyword);
    if (idx == -1) return 0;
    
    IndexItem* item = &InvertedIndex.items[idx];
    int count = (item->docCount < maxResult) ? item->docCount : maxResult;
    for (int i = 0; i < count; i++) result[i] = item->docIds[i];
    return count;
}

int indexSearchAnd(char keys[MAX_KEY_NUM][MAX_KEY_LEN], int keyNum, int result[], int maxResult) {
    if (keyNum == 0) return 0;
    
    int temp[MAX_DOCS];
    int count = getDocsByKeyword(keys[0], temp, MAX_DOCS);
    
    for (int i = 1; i < keyNum; i++) {
        int current[MAX_DOCS];
        int currentCount = getDocsByKeyword(keys[i], current, MAX_DOCS);
        
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
    
    int finalCount = (count < maxResult) ? count : maxResult;
    for (int i = 0; i < finalCount; i++) result[i] = temp[i];
    return finalCount;
}

int indexSearchOr(char keys[MAX_KEY_NUM][MAX_KEY_LEN], int keyNum, int result[], int maxResult) {
    int temp[MAX_DOCS] = {0};
    int count = 0;
    
    for (int i = 0; i < keyNum; i++) {
        int current[MAX_DOCS];
        int currentCount = getDocsByKeyword(keys[i], current, MAX_DOCS);
        
        for (int j = 0; j < currentCount; j++) {
            int exists = 0;
            for (int k = 0; k < count; k++) {
                if (temp[k] == current[j]) { exists = 1; break; }
            }
            if (!exists && count < MAX_DOCS) temp[count++] = current[j];
        }
    }
    
    int finalCount = (count < maxResult) ? count : maxResult;
    for (int i = 0; i < finalCount; i++) result[i] = temp[i];
    return finalCount;
}

// 倒排索引检索主入口（带排序）
void indexSearch() {
    char input[500], keys[MAX_KEY_NUM][MAX_KEY_LEN];
    printf("\n========== 倒排索引检索 ==========\n");
    printf("请输入关键词（空格分隔）：");
    getchar(); fgets(input, 500, stdin);
    int n = splitChinese(input, keys);
    if (!n) { printf("[错误] 未输入有效关键词\n"); return; }

    int mode;
    printf("请选择检索模式：1-全部包含(AND)  2-任意包含(OR) ：");
    if (scanf("%d", &mode) != 1 || (mode != 1 && mode != 2)) {
        printf("[错误] 输入无效，默认使用OR模式\n");
        mode = 2;
        while (getchar() != '\n'); // 清空缓冲区
    }

    int docIds[MAX_DOCS];
    int matchCount;
    
    if (mode == 1) {
        matchCount = indexSearchAnd(keys, n, docIds, MAX_DOCS);
    } else {
        matchCount = indexSearchOr(keys, n, docIds, MAX_DOCS);
    }

    // 计算得分并排序
    SearchResult results[MAX_DOCS];
    for (int i = 0; i < matchCount; i++) {
        results[i].docIdx = docIds[i];
        results[i].score = calcDocScore(docIds[i], keys, n);
    }
    sortResults(results, matchCount);

    printf("\n---------- 检索结果（按相关度排序） ----------\n");
    if (matchCount == 0) {
        printf("未找到匹配的文档\n");
    } else {
        for (int i = 0; i < matchCount; i++) {
            int idx = results[i].docIdx;
            printf("[%d] 相关度得分：%d\n", i+1, results[i].score);
            printf("    ID：%s\n", DocLibrary.docs[idx].id);
            printf("    标题：%s", DocLibrary.docs[idx].title);
            printf("----------------------------------------\n");
        }
        printf("共匹配 %d 篇文档\n", matchCount);
    }
}

// ===================== 线性检索（带排序，保留用于对比） =====================
void linearSearch() {
    char input[500], keys[MAX_KEY_NUM][MAX_KEY_LEN];
    printf("\n========== 线性遍历检索 ==========\n");
    printf("请输入关键词（空格分隔）：");
    getchar(); fgets(input, 500, stdin);
    int n = splitChinese(input, keys);
    if (!n) { printf("[错误] 未输入有效关键词\n"); return; }

    int mode;
    printf("请选择检索模式：1-全部包含  2-任意包含 ：");
    scanf("%d", &mode);

    SearchResult results[MAX_DOCS];
    int matchCount = 0;

    for (int i = 0; i < DocLibrary.count; i++) {
        int ok = 1;
        if (mode == 1) {
            for (int k = 0; k < n; k++) {
                if (!KMP(DocLibrary.docs[i].title, keys[k]) && !KMP(DocLibrary.docs[i].content, keys[k])) {
                    ok = 0; break;
                }
            }
        } else {
            ok = 0;
            for (int k = 0; k < n; k++) {
                if (KMP(DocLibrary.docs[i].title, keys[k]) || KMP(DocLibrary.docs[i].content, keys[k])) {
                    ok = 1; break;
                }
            }
        }
        if (ok) {
            results[matchCount].docIdx = i;
            results[matchCount].score = calcDocScore(i, keys, n);
            matchCount++;
        }
    }

    sortResults(results, matchCount);
    printf("\n---------- 检索结果（按相关度排序） ----------\n");
    for (int i = 0; i < matchCount; i++) {
        int idx = results[i].docIdx;
        printf("[%d] 得分：%d | ID：%s | 标题：%s", 
               i+1, results[i].score, 
               DocLibrary.docs[idx].id, DocLibrary.docs[idx].title);
    }
    printf("共匹配 %d 篇文档\n", matchCount);
}

// ===================== 图算法模块 =====================
int calcDocSimilarity(int doc1, int doc2) {
    if (doc1 == doc2) return 0;
    int common = 0;
    for (int i = 0; i < InvertedIndex.count; i++) {
        IndexItem* item = &InvertedIndex.items[i];
        int has1 = 0, has2 = 0;
        for (int j = 0; j < item->docCount; j++) {
            if (item->docIds[j] == doc1) has1 = 1;
            if (item->docIds[j] == doc2) has2 = 1;
        }
        if (has1 && has2) common++;
    }
    return common;
}

void buildDocGraph() {
    for (int i = 0; i < MAX_DOCS; i++) {
        for (int j = 0; j < MAX_DOCS; j++) docGraph[i][j] = 0;
    }
    for (int i = 0; i < DocLibrary.count; i++) {
        for (int j = i+1; j < DocLibrary.count; j++) {
            int sim = calcDocSimilarity(i, j);
            if (sim >= SIMILARITY_THRESHOLD) {
                docGraph[i][j] = sim;
                docGraph[j][i] = sim;
            }
        }
    }
}

void dfsRecommend(int docIdx, int recommend[], int* recCount) {
    visited[docIdx] = 1;
    for (int i = 0; i < DocLibrary.count; i++) {
        if (docGraph[docIdx][i] > 0 && !visited[i] && *recCount < MAX_RECOMMEND) {
            recommend[*recCount] = i;
            (*recCount)++;
            dfsRecommend(i, recommend, recCount);
        }
    }
}

void recommendSimilarDocs() {
    char id[MAX_ID_LEN];
    printf("\n========== 相关文档推荐 ==========\n");
    printf("请输入目标文档ID：");
    scanf("%s", id);
    
    int docIdx = -1;
    for (int i = 0; i < DocLibrary.count; i++) {
        if (myStrcmp(DocLibrary.docs[i].id, id) == 0) { docIdx = i; break; }
    }
    
    if (docIdx == -1) { printf("[错误] 未找到该文档\n"); return; }
    
    buildDocGraph();
    for (int i = 0; i < MAX_DOCS; i++) visited[i] = 0;
    
    int recommend[MAX_RECOMMEND];
    int recCount = 0;
    dfsRecommend(docIdx, recommend, &recCount);
    
    printf("\n目标文档：%s - %s", DocLibrary.docs[docIdx].id, DocLibrary.docs[docIdx].title);
    printf("---------- 推荐结果 ----------\n");
    if (recCount == 0) {
        printf("暂无相关文档推荐\n");
        return;
    }
    for (int i = 0; i < recCount; i++) {
        int idx = recommend[i];
        printf("[%d] %s - %s", i+1, DocLibrary.docs[idx].id, DocLibrary.docs[idx].title);
        printf("    相似度：%d\n", docGraph[docIdx][idx]);
    }
}

void buildKeywordGraph() {
    for (int i = 0; i < MAX_INDEX_ITEMS; i++) {
        for (int j = 0; j < MAX_INDEX_ITEMS; j++) keywordGraph[i][j] = 0;
    }
    for (int d = 0; d < DocLibrary.count; d++) {
        char keys[MAX_KEY_NUM][MAX_KEY_LEN];
        char contentCopy[MAX_CONTENT_LEN];
        myStrcpy(contentCopy, DocLibrary.docs[d].content);
        int keyNum = splitChinese(contentCopy, keys);
        
        for (int i = 0; i < keyNum; i++) {
            int idx1 = findIndexItem(keys[i]);
            if (idx1 == -1) continue;
            for (int j = i+1; j < keyNum; j++) {
                int idx2 = findIndexItem(keys[j]);
                if (idx2 == -1) continue;
                keywordGraph[idx1][idx2]++;
                keywordGraph[idx2][idx1]++;
            }
        }
    }
}

void recommendKeywords() {
    char keyword[MAX_KEY_LEN];
    printf("\n========== 相关关键词推荐 ==========\n");
    printf("请输入关键词：");
    scanf("%s", keyword);
    
    int idx = findIndexItem(keyword);
    if (idx == -1) { printf("[错误] 未找到该关键词\n"); return; }
    
    buildKeywordGraph();
    printf("\n目标关键词：%s\n", keyword);
    printf("---------- 相关关键词 ----------\n");
    int found = 0;
    for (int i = 0; i < InvertedIndex.count; i++) {
        if (keywordGraph[idx][i] > 0) {
            printf("%s (共现 %d 次)\n", InvertedIndex.items[i].keyword, keywordGraph[idx][i]);
            found = 1;
        }
    }
    if (!found) printf("暂无相关关键词\n");
}

void showDocGraph() {
    buildDocGraph();
    printf("\n========== 文档关联图 ==========\n");
    int edgeCount = 0;
    for (int i = 0; i < DocLibrary.count; i++) {
        for (int j = i+1; j < DocLibrary.count; j++) {
            if (docGraph[i][j] > 0) edgeCount++;
        }
    }
    printf("总节点数：%d，总边数：%d\n", DocLibrary.count, edgeCount);
    printf("\n邻接关系：\n");
    for (int i = 0; i < DocLibrary.count; i++) {
        printf("文档 %s：", DocLibrary.docs[i].id);
        int hasEdge = 0;
        for (int j = 0; j < DocLibrary.count; j++) {
            if (docGraph[i][j] > 0) {
                printf("%s(相似度%d) ", DocLibrary.docs[j].id, docGraph[i][j]);
                hasEdge = 1;
            }
        }
        if (!hasEdge) printf("无连接");
        printf("\n");
    }
}

// ===================== 文档管理模块 =====================
void initLibrary() { DocLibrary.count = 0; }

int findDocById(const char* id) {
    for (int i = 0; i < DocLibrary.count; i++)
        if (!myStrcmp(DocLibrary.docs[i].id, id)) return i;
    return -1;
}

void addDoc() {
    if (DocLibrary.count >= MAX_DOCS) {
        printf("[错误] 文档库已满，最多支持 %d 篇\n", MAX_DOCS);
        return;
    }
    Document d;
    printf("\n========== 新增文档 ==========\n");
    printf("请输入文档ID：");
    scanf("%s", d.id); getchar();
    if (findDocById(d.id) != -1) {
        printf("[错误] 该ID已存在\n");
        return;
    }
    printf("请输入文档标题：");
    fgets(d.title, MAX_TITLE_LEN, stdin);
    printf("请输入文档内容：");
    fgets(d.content, MAX_CONTENT_LEN, stdin);
    
    DocLibrary.docs[DocLibrary.count] = d;
    indexDocument(DocLibrary.count);
    DocLibrary.count++;
    printf("[成功] 文档添加完成\n");
}

void showAllDocs() {
    printf("\n========== 全部文档列表 ==========\n");
    if (!DocLibrary.count) {
        printf("文档库为空\n");
        return;
    }
    for (int i = 0; i < DocLibrary.count; i++) {
        printf("[%d] ID：%s\n", i+1, DocLibrary.docs[i].id);
        printf("    标题：%s", DocLibrary.docs[i].title);
        printf("----------------------------------------\n");
    }
    printf("共 %d 篇文档\n", DocLibrary.count);
}

void modifyDoc() {
    char id[MAX_ID_LEN];
    printf("\n========== 修改文档 ==========\n");
    printf("请输入要修改的文档ID：");
    scanf("%s", id);
    int idx = findDocById(id);
    if (idx == -1) { printf("[错误] 文档不存在\n"); return; }
    
    getchar();
    printf("请输入新标题：");
    fgets(DocLibrary.docs[idx].title, MAX_TITLE_LEN, stdin);
    printf("请输入新内容：");
    fgets(DocLibrary.docs[idx].content, MAX_CONTENT_LEN, stdin);
    
    removeDocFromIndex(idx);
    indexDocument(idx);
    printf("[成功] 文档修改完成\n");
}

void deleteDoc() {
    char id[MAX_ID_LEN];
    printf("\n========== 删除文档 ==========\n");
    printf("请输入要删除的文档ID：");
    scanf("%s", id);
    int idx = findDocById(id);
    if (idx == -1) { printf("[错误] 文档不存在\n"); return; }
    
    removeDocFromIndex(idx);
    for (int i = idx; i < DocLibrary.count - 1; i++) {
        DocLibrary.docs[i] = DocLibrary.docs[i + 1];
    }
    DocLibrary.count--;
    rebuildIndex();
    printf("[成功] 文档删除完成\n");
}

// 版本6新增：清空文档库
void clearLibrary() {
    char confirm;
    printf("\n[警告] 确定要清空所有文档吗？(y/n)：");
    getchar();
    scanf("%c", &confirm);
    if (confirm == 'y' || confirm == 'Y') {
        DocLibrary.count = 0;
        initIndex();
        printf("[成功] 文档库已清空\n");
    } else {
        printf("已取消操作\n");
    }
}

// ===================== 文件操作模块 =====================
void saveDocsToFile() {
    FILE* f = fopen(FILE_NAME, "w");
    if (!f) { printf("[错误] 文件打开失败，无法保存\n"); return; }
    fprintf(f, "%d\n", DocLibrary.count);
    for (int i = 0; i < DocLibrary.count; i++)
        fprintf(f, "%s\n%s%s", DocLibrary.docs[i].id, DocLibrary.docs[i].title, DocLibrary.docs[i].content);
    fclose(f);
    printf("[成功] 数据已保存到 %s\n", FILE_NAME);
}

void loadDocsFromFile() {
    FILE* f = fopen(FILE_NAME, "r");
    if (!f) {
        printf("[提示] 未找到历史数据文件，新建空文档库\n");
        return;
    }
    fscanf(f, "%d", &DocLibrary.count);
    getchar();
    for (int i = 0; i < DocLibrary.count; i++) {
        fgets(DocLibrary.docs[i].id, MAX_ID_LEN, f);
        DocLibrary.docs[i].id[myStrlen(DocLibrary.docs[i].id) - 1] = 0;
        fgets(DocLibrary.docs[i].title, MAX_TITLE_LEN, f);
        fgets(DocLibrary.docs[i].content, MAX_CONTENT_LEN, f);
    }
    fclose(f); 
    rebuildIndex();
    printf("[成功] 从文件加载 %d 篇文档\n", DocLibrary.count);
}

// ===================== 性能测试模块 =====================
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

    printf("\n========== BF VS KMP 算法效率测试 ==========\n");
    printf("测试文本长度：%d，关键词长度：%d，循环10000次\n", myStrlen(text), myStrlen(pattern));

    // BF算法
    start = clock();
    for (int loop = 0; loop < 10000; loop++) {
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
    printf("BF  算法耗时：%ld\n", t1);

    // KMP算法
    start = clock();
    for (int loop = 0; loop < 10000; loop++) KMP(text, pattern);
    end = clock();
    t2 = end - start;
    printf("KMP 算法耗时：%ld\n", t2);

    if (t2 < t1) {
        printf("结论：KMP 效率提升 %.1f 倍\n", (double)t1 / t2);
    } else {
        printf("结论：短文本下两者差距较小\n");
    }
}

void testSearchPerformance() {
    rebuildIndex();
    char keys[MAX_KEY_NUM][MAX_KEY_LEN] = {"a", "b", "c"};
    int keyNum = 3;
    
    clock_t start, end;
    long t1, t2;
    
    printf("\n========== 线性检索 VS 倒排索引 效率对比 ==========\n");
    printf("测试文档数：%d，关键词数：%d，循环1000次\n", DocLibrary.count, keyNum);
    
    start = clock();
    for (int loop = 0; loop < 1000; loop++) {
        int cnt = 0;
        for (int d = 0; d < DocLibrary.count; d++) {
            int match = 1;
            for (int k = 0; k < keyNum; k++) {
                if (!KMP(DocLibrary.docs[d].title, keys[k]) && !KMP(DocLibrary.docs[d].content, keys[k])) {
                    match = 0; break;
                }
            }
            if (match) cnt++;
        }
    }
    end = clock();
    t1 = end - start;
    printf("线性检索耗时：%ld\n", t1);
    
    start = clock();
    for (int loop = 0; loop < 1000; loop++) {
        int result[MAX_DOCS];
        indexSearchAnd(keys, keyNum, result, MAX_DOCS);
    }
    end = clock();
    t2 = end - start;
    printf("倒排索引耗时：%ld\n", t2);
    
    if (t2 < t1) {
        printf("结论：倒排索引速度提升 %.1f 倍\n", (double)t1 / t2);
    } else {
        printf("结论：文档量过少时索引优势不明显\n");
    }
}

// 版本6新增：系统信息总览
void showSystemInfo() {
    buildDocGraph();
    int edgeCount = 0;
    for (int i = 0; i < DocLibrary.count; i++) {
        for (int j = i+1; j < DocLibrary.count; j++) {
            if (docGraph[i][j] > 0) edgeCount++;
        }
    }
    
    printf("\n========== 系统信息总览 ==========\n");
    printf("文档总数：%d / %d\n", DocLibrary.count, MAX_DOCS);
    printf("索引词条数：%d / %d\n", InvertedIndex.count, MAX_INDEX_ITEMS);
    printf("文档关联图：节点 %d 个，边 %d 条\n", DocLibrary.count, edgeCount);
    printf("数据文件：%s\n", FILE_NAME);
    printf("最大单篇内容长度：%d 字符\n", MAX_CONTENT_LEN);
    printf("==================================\n");
}

// ===================== 主菜单与主函数 =====================
void showWelcome() {
    printf("==============================================\n");
    printf("        简易搜索引擎系统  版本6.0 最终版        \n");
    printf("  数据结构与算法设计实践专题 课程设计项目  \n");
    printf("==============================================\n");
}

void showMenu() {
    printf("\n");
    printf("================ 主菜单 ================\n");
    printf(" 1. 新增文档        2. 查看全部文档\n");
    printf(" 3. 修改文档        4. 删除文档\n");
    printf(" 5. 保存到文件      6. 从文件加载\n");
    printf(" 7. 清空文档库      8. 系统信息总览\n");
    printf("----------------------------------------\n");
    printf(" 9. 倒排索引检索   10. 线性遍历检索\n");
    printf("11. 相关文档推荐   12. 相关关键词推荐\n");
    printf("13. 查看文档关联图\n");
    printf("----------------------------------------\n");
    printf("14. BF/KMP算法对比 15. 检索效率对比\n");
    printf("16. 重建倒排索引\n");
    printf("----------------------------------------\n");
    printf(" 0. 退出程序\n");
    printf("========================================\n");
    printf("请输入选项编号：");
}

int main() {
    initLibrary();
    initIndex();
    
    showWelcome();
    loadDocsFromFile();
    
    int op;
    while (1) {
        showMenu();
        if (scanf("%d", &op) != 1) {
            printf("[错误] 请输入数字选项！\n");
            while (getchar() != '\n'); // 清空输入缓冲区
            continue;
        }

        switch (op) {
            case 1: addDoc(); break;
            case 2: showAllDocs(); break;
            case 3: modifyDoc(); break;
            case 4: deleteDoc(); break;
            case 5: saveDocsToFile(); break;
            case 6: loadDocsFromFile(); break;
            case 7: clearLibrary(); break;
            case 8: showSystemInfo(); break;
            case 9: indexSearch(); break;
            case 10: linearSearch(); break;
            case 11: recommendSimilarDocs(); break;
            case 12: recommendKeywords(); break;
            case 13: showDocGraph(); break;
            case 14: testAlgorithmPerformance(); break;
            case 15: testSearchPerformance(); break;
            case 16: rebuildIndex(); break;
            case 0:
                printf("\n感谢使用，程序退出\n");
                return 0;
            default:
                printf("[错误] 选项不存在，请重新输入\n");
        }
    }
    return 0;
}

