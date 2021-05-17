# distributeddatamgr_appdatamgr

-   [简介](#section11660541593)
-   [目录](#section1464106163817)
-   [约束](#section1718733212019)
-   [软件架构](#section159991817144514)
-   [接口](#section11510542164514)
-   [使用](#section1685211117463)
-   [涉及仓](#section10365113863719)

## 简介<a name="section11660541593"></a>
数据管理服务为应用程序和用户提供更加便捷、高效和安全的数据管理能力。降低开发成本，打造应用跨设备运行一致、流畅的用户体验。
> 当前先支持轻量键值（KV）本地数据存储能力，后续会逐步支持其他更丰富的数据类型。
>
> 轻量键值（KV）数据：数据有结构，文件轻量，具有事务性（未来支持），单独提供一套专用的键值对接口

![输入图片说明](https://images.gitee.com/uploads/images/2021/0422/193406_a3e03a96_8046977.png "屏幕截图.png")

轻量级KV数据库依托当前公共基础库提供的KV存储能力开发，为应用提供键值对参数管理能力。在有进程的平台上，KV存储提供的参数管理，供单进程访问不能被其他进程使用。在此类平台上，KV存储作为基础库加载在应用进程，以保障不被其他进程访问。

分布式数据管理服务在不同平台上，将数据操作接口形成抽象层用来统一进行文件操作，使厂商不需要关注不同芯片平台文件系统的差异。

## 目录<a name="section1464106163817"></a>
> 待代码开发完毕后补充

## 约束<a name="section1718733212019"></a>
### 轻量键值（KV）数据
- 依赖平台具有正常的文件创建、读写删除修改、锁等能力，针对不同平台（如LiteOS-M内核、LiteOS-A内核等）尽可能表现接口语义功能的不变
- 由于平台能力差异数据库能力需要做相应裁剪，其中不同平台内部实现可能不同

## 软件架构<a name="section159991817144514"></a>
### 轻量键值（KV）数据
KV存储能力继承自公共基础库原始设计，在原有能力基础上进行增强，新增提供数据删除及二进制value读写能力的同时，保证操作的原子性；为区别平台差异，将依赖平台差异的内容单独抽象，由对应产品平台提供。
>- 轻量系统普遍性能有限，内存及计算能力不足，对于数据管理的场景大多读多写少，且内存占用敏感；
>- 平台使用的文件操作接口是由文件系统提供，一般来说文件操作接口本身并不是进程安全的，请格外注意；
>- 轻量系统，存在不具备锁能力的情况，不提供锁的机制，并发由业务保证，若需要提供有锁机制，则需要提供hook，由业务进行注册。

## 接口<a name="section11510542164514"></a>
- **轻量KV存储**

    ```
    typedef struct DBM *KVStoreHandle;
    // storeFullPath为合法目录，创建的KV将已此目录创建条目
    // 传入空串则以当前目录创建
    int DBM_GetKVStore(const char* storeFullPath, KVStoreHandle* kvStore);
    
    int DBM_Get(KVStoreHandle db, const char* key, void* value, unsigned int count, unsigned int* realValueLen);
    int DBM_Put(KVStoreHandle db, const char* key, void* value, unsigned int len);
    int DBM_Delete(KVStoreHandle db, const char* key);
    
    int DBM_CloseKVStore(KVStoreHandle db);
    // 请确保删除数据库前已关闭该目录对应的所有数据库对象
    int DBM_DeleteKVStore(const char* storeFullPath);
    ```
## 使用<a name="section1685211117463"></a>

- **轻量KV存储**

    ```
    // 创建数据库
    const char storeFullPath[] = "";  // 目录或空字符串
    KVStoreHandle kvStore = NULL;
    int ret = DBM_GetKVStore(storeFullPath, &kvStore);
    
    // 插入或修改数据
    char key[] = "rw.sys.version";
    struct {
        int num;
        char content[200];
    } value;
    memset_s(&value, sizeof(value), 0, sizeof(value));
    value.num = 1;
    strcpy_s(value.content, sizeof(value.content), "Hello world !");
    ret = DBM_Put(kvStore, key, (void*)&value, sizeof(value));
    
    // 读取数据
    memset_s(&value, sizeof(value), 0, sizeof(value));
    unsigned int realValLen = 0;
    ret = DBM_Get(g_KVStoreHandle, key, &value, sizeof(value), &realValLen);
    
    // 删除数据
    ret = DBM_Delete(kvStore, key);
    
    // 关闭数据库
    ret = DBM_CloseKVStore(kvStore);
    
    // 删除数据库
    ret = DBM_DeleteKVStore(storeFullPath);
    
    ```

## 涉及仓<a name="section10365113863719"></a>
distributeddatamgr_appdatamgr