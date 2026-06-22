#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// ===================== 配置常量 =====================
#define MAX_DOCS 100       // 最大文档数量
#define MAX_ID_LEN 20      // 文档ID最大长度
#define MAX_TITLE_LEN 100  // 标题最大长度
#define MAX_CONTENT_LEN 2000 // 内容最大长度
#define MAX_KEY_LEN 50     // 关键词最大长度
#define MAX_KEY_NUM 10     // 最多支持10个关键词
#define MAX_INDEX_ITEMS 500  // 倒排索引最大词条数
#define MAX_DOCS_PER_KEY 50  // 每个关键词最多关联文档数
#define MAX_RECOMMEND 5  // 最多推荐5篇相关文档
#define SIMILARITY_THRESHOLD 2  // 相似度阈值，共同关键词>=2则建立边
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

// ===================== 版本5 新增：图数据结构 =====================
// 文档关联图：邻接矩阵存储，值为相似度（共同关键词数量）
int docGraph[MAX_DOCS][MAX_DOCS];

// 关键词共现图：邻接矩阵存储，值为共现次数
int keywordGraph[MAX_INDEX_ITEMS][MAX_INDEX_ITEMS];

// 图遍历访问标记
int visited[MAX_DOCS];


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

// 中文分词（简单实现：按空格、标点分割）(版本5修改）
int splitChinese(char* in, char k[MAX_KEY_NUM][MAX_KEY_LEN]) {
    int c = 0, idx = 0, len = myStrlen(in);
    char separators[] = " ,.!?，。！？；：;:";
    
    for (int i = 0; i < len && c < MAX_KEY_NUM; i++) {
        int isSep = 0;
        for (int j = 0; separators[j] != '\0'; j++) {
            if (in[i] == separators[j]) {
                isSep = 1;
                break;
            }
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

// ===================== 版本5 新增：图算法核心函数 =====================
// 计算两篇文档的相似度（共同关键词数量）
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

// 构建文档关联图
void buildDocGraph() {
    // 初始化图
    for (int i = 0; i < MAX_DOCS; i++) {
        for (int j = 0; j < MAX_DOCS; j++) {
            docGraph[i][j] = 0;
        }
    }
    
    // 计算所有文档对的相似度
    for (int i = 0; i < DocLibrary.count; i++) {
        for (int j = i+1; j < DocLibrary.count; j++) {
            int sim = calcDocSimilarity(i, j);
            if (sim >= SIMILARITY_THRESHOLD) {
                docGraph[i][j] = sim;
                docGraph[j][i] = sim; // 无向图
            }
        }
    }
    
    printf("✅ 文档关联图构建完成\n");
    printf("📊 文档数：%d，边数：", DocLibrary.count);
    int edgeCount = 0;
    for (int i = 0; i < DocLibrary.count; i++) {
        for (int j = i+1; j < DocLibrary.count; j++) {
            if (docGraph[i][j] > 0) edgeCount++;
        }
    }
    printf("%d\n", edgeCount);
}

// DFS深度优先遍历，收集相关文档
void dfsRecommend(int docIdx, int recommend[], int* recCount) {
    visited[docIdx] = 1;
    
    // 遍历所有相邻节点
    for (int i = 0; i < DocLibrary.count; i++) {
        if (docGraph[docIdx][i] > 0 && !visited[i] && *recCount < MAX_RECOMMEND) {
            recommend[*recCount] = i;
            (*recCount)++;
            dfsRecommend(i, recommend, recCount);
        }
    }
}

// 基于图的相关文档推荐
void recommendSimilarDocs() {
    char id[MAX_ID_LEN];
    printf("请输入要推荐的文档ID：");
    scanf("%s", id);
    
    int docIdx = -1;
    for (int i = 0; i < DocLibrary.count; i++) {
        if (myStrcmp(DocLibrary.docs[i].id, id) == 0) {
            docIdx = i;
            break;
        }
    }
    
    if (docIdx == -1) {
        printf("❌ 未找到该文档\n");
        return;
    }
    
    // 构建图
    buildDocGraph();
    
    // 初始化访问标记
    for (int i = 0; i < MAX_DOCS; i++) {
        visited[i] = 0;
    }
    
    int recommend[MAX_RECOMMEND];
    int recCount = 0;
    
    // DFS推荐
    dfsRecommend(docIdx, recommend, &recCount);
    
    printf("\n===== 相关文档推荐 =====\n");
    printf("目标文档：%s - %s", DocLibrary.docs[docIdx].id, DocLibrary.docs[docIdx].title);
    
    if (recCount == 0) {
        printf("❌ 没有找到相关文档\n");
        return;
    }
    
    for (int i = 0; i < recCount; i++) {
        int idx = recommend[i];
        printf("推荐%d：%s - %s", i+1, DocLibrary.docs[idx].id, DocLibrary.docs[idx].title);
        printf("相似度：%d\n", docGraph[docIdx][idx]);
    }
}

// 构建关键词共现图
void buildKeywordGraph() {
    // 初始化图
    for (int i = 0; i < MAX_INDEX_ITEMS; i++) {
        for (int j = 0; j < MAX_INDEX_ITEMS; j++) {
            keywordGraph[i][j] = 0;
        }
    }
    
    // 遍历所有文档，统计关键词共现
    for (int d = 0; d < DocLibrary.count; d++) {
        char keys[MAX_KEY_NUM][MAX_KEY_LEN];
        char contentCopy[MAX_CONTENT_LEN];
        myStrcpy(contentCopy, DocLibrary.docs[d].content);
        int keyNum = splitChinese(contentCopy, keys);
        
        // 统计该文档中所有关键词对的共现
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
    
    printf("✅ 关键词共现图构建完成\n");
}

// 相关关键词推荐
void recommendKeywords() {
    char keyword[MAX_KEY_LEN];
    printf("请输入关键词：");
    scanf("%s", keyword);
    
    int idx = findIndexItem(keyword);
    if (idx == -1) {
        printf("❌ 未找到该关键词\n");
        return;
    }
    
    buildKeywordGraph();
    
    printf("\n===== 相关关键词推荐 =====\n");
    printf("目标关键词：%s\n", keyword);
    
    int found = 0;
    for (int i = 0; i < InvertedIndex.count; i++) {
        if (keywordGraph[idx][i] > 0) {
            printf("%s (共现%d次)\n", InvertedIndex.items[i].keyword, keywordGraph[idx][i]);
            found = 1;
        }
    }
    
    if (!found) {
        printf("❌ 没有找到相关关键词\n");
    }
}

// 可视化文档关联图
void showDocGraph() {
    buildDocGraph();
    
    printf("\n===== 文档关联图邻接矩阵 =====\n");
    printf("   ");
    for (int i = 0; i < DocLibrary.count; i++) {
        printf("%3d", i);
    }
    printf("\n");
    
    for (int i = 0; i < DocLibrary.count; i++) {
        printf("%3d", i);
        for (int j = 0; j < DocLibrary.count; j++) {
            printf("%3d", docGraph[i][j]);
        }
        printf("\n");
    }
    
    printf("\n===== 文档关联图拓扑结构 =====\n");
    for (int i = 0; i < DocLibrary.count; i++) {
        printf("文档%d(%s) 连接：", i, DocLibrary.docs[i].id);
        for (int j = 0; j < DocLibrary.count; j++) {
            if (docGraph[i][j] > 0) {
                printf("文档%d(相似度%d) ", j, docGraph[i][j]);
            }
        }
        printf("\n");
    }
}

// ===================== 文档管理功能 =====================
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
    removeDocFromIndex(idx);
    printf("新标题："); fgets(DocLibrary.docs[idx].title, MAX_TITLE_LEN, stdin);
    printf("新内容："); fgets(DocLibrary.docs[idx].content, MAX_CONTENT_LEN, stdin);
    indexDocument(idx);
    printf("修改成功\n");
}

void deleteDoc() {
    char id[MAX_ID_LEN]; printf("ID："); scanf("%s", id);
    int idx = findDocById(id);
    if (idx == -1) { printf("不存在\n"); return; }
    removeDocFromIndex(idx);
    for (int i = idx; i < DocLibrary.count - 1; i++) {
        DocLibrary.docs[i] = DocLibrary.docs[i + 1];
    }
    DocLibrary.count--;
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
    rebuildIndex();
    printf("加载成功\n");
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

// ===================== 效率测试 =====================
void testSearchPerformance() {
    rebuildIndex();
    
    char keys[MAX_KEY_NUM][MAX_KEY_LEN] = {"a", "b", "c"};
    int keyNum = 3;
    
    clock_t start, end;
    long t1, t2;
    
    printf("\n===== 线性检索 VS 倒排索引检索 效率对比 =====\n");
    printf("测试文档数：%d\n", DocLibrary.count);
    printf("测试关键词数：%d\n", keyNum);
    
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
        printf("✅ 结论：倒排索引速度提升 %.1f 倍！\n", speedup);
    } else {
        printf("⚠  结论：文档量过少时，索引优势不明显\n");
    }
}

// ------------------------------
// 菜单（版本5新增图应用选项）
// ------------------------------
void showMenu() {
    printf("\n===== 简易搜索引擎 版本5：图应用增强版 =====\n");
    printf("1.新增文档  2.查看所有  3.修改文档  4.删除文档\n");
    printf("5.保存文件  6.加载文件  7.倒排索引检索\n");
    printf("8.【图应用】相关文档推荐\n");
    printf("9.【图应用】相关关键词推荐\n");
    printf("10.【图应用】可视化文档关联图\n");
    printf("11.检索效率对比  12.重建索引\n");
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
            case 7: indexSearch(); break;
            case 8: recommendSimilarDocs(); break;
            case 9: recommendKeywords(); break;
            case 10: showDocGraph(); break;
            case 11: testSearchPerformance(); break;
            case 12: rebuildIndex(); break;
            case 0: return 0;
            default: printf("错误\n");
        }
    }
}


