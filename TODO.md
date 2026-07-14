# TODO — TCP + Concurrency (Eş Zamanlılık)

Bu dosya, komutları TCP üzerinden alıp hash table'da işlerken gereken
senkronizasyon (mutex / atomic) işlerini ve nasıl yapılacağını örneklerle anlatır.

> **Özet karar:** Hash table paylaşımlı olduğu için, thread-per-connection modeli
> seçersek **mutex ZORUNLU**. Tek-thread event loop (epoll) seçersek mutex GEREKMEZ.
> Başlangıç için öneri: **tek global mutex** ile eval'i sarmalamak (basit + doğru).

---

## 0. Önce tasarım kararı: Threading modeli

| Model | Mutex gerekir mi? | Zorluk | Not |
|---|---|---|---|
| **A. Tek thread + epoll/poll** (Redis modeli) | Hayır | Orta (non-blocking IO) | En hızlı, en temiz |
| **B. Thread-per-connection** (`pthread`) | **Evet** | Kolay başlar | Hash table kilitlenmeli |
| **C. Thread pool** | **Evet** | Orta | B ile aynı kilitleme mantığı |

Aşağıdaki TODO'lar **Model B** (thread-per-connection) içindir çünkü en olası yol bu.
Model A seçersen 3–6 arası kilit maddelerini atla.

---

## 1. [BUG] `tcp_listen` return değeri eksik  — `net/tcp.c:42`

`tcp_listen` başarı durumunda `return TCP_SUCCESS;` yapmıyor (undefined behavior).

```c
int tcp_listen(int sd, int queue)
{
    if (listen(sd, queue) == -1) { LOG(stderr,"Listen error\n"); return TCP_LIST_ERR; }
    return TCP_SUCCESS;   // <-- EKLE
}
```

---

## 2. [TCP] Accept döngüsü + thread açma  — `net/tcp.c`

Şu an sadece socket/bind/listen var. `accept()` döngüsü ve client handler yok.

```c
// her client için thread
typedef struct { int fd; HashTable *db; } ClientCtx;

void *handle_client(void *arg) {
    ClientCtx *c = arg;
    char buf[4096];
    ssize_t n;
    while ((n = recv(c->fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        // parse + eval  (aşağıdaki kilit maddesine bak)
        // sonucu stdout yerine c->fd'ye yaz:  send(c->fd, out, len, 0);
    }
    close(c->fd);
    free(c);
    return NULL;
}

int tcp_accept_loop(int sd, HashTable *db) {
    for (;;) {
        int cfd = accept(sd, NULL, NULL);
        if (cfd == -1) continue;
        ClientCtx *c = malloc(sizeof *c);
        c->fd = cfd; c->db = db;
        pthread_t t;
        pthread_create(&t, NULL, handle_client, c);
        pthread_detach(t);   // join etmeyeceğiz
    }
}
```

---

## 3. [KİLİT] Hash table'a global mutex ekle  — `core/hash_table/hash-table.h/.c`

**Neden:** `ht_set` bucket'ın başına ekleme yaparken (`hash-table.c:32`) ve
`ht_delete` node `free()` ederken (`hash-table.c:45`) iki thread çakışırsa:
bozuk linked list, kayıp kayıt, use-after-free, çökme.

`HashTable` struct'ına bir mutex koy:

```c
// hash-table.h
#include <pthread.h>

typedef struct {
    Entry *buckets[TABLE_SIZE];
    int    count;
    pthread_mutex_t lock;   // <-- EKLE
} HashTable;
```

`ht_create` içinde init et, `ht_destroy` içinde yok et:

```c
HashTable *ht_create(void) {
    HashTable *ht = calloc(1, sizeof(HashTable));
    pthread_mutex_init(&ht->lock, NULL);
    return ht;
}
void ht_destroy(HashTable *ht) {
    // ... mevcut free döngüsü ...
    pthread_mutex_destroy(&ht->lock);
    free(ht);
}
```

---

## 4. [KİLİT] KRİTİK: Kilidi KOMUT seviyesinde tut, ht_* içinde DEĞİL

**Neden ht_* içinde tek tek kilitlemek YETMEZ:**
`APPEND` (eval.c:31-37) şunu yapıyor → `ht_get` (oku) + hesapla + `ht_set` (yaz).
Her ikisini ayrı ayrı kilitlersen, arada başka thread araya girip değeri
değiştirebilir → **kayıp güncelleme** (lost update). Aynı sorun
`RENAME` (get+set+del) ve `MSET`/`MGET` için de geçerli.

**Çözüm:** Tüm komutu (eval'i) tek kilit altında çalıştır. En basit ve doğru yol.

```c
// eval çağrısını saran yer (net handler veya run())
pthread_mutex_lock(&db->lock);
int stop = eval(node, db);
pthread_mutex_unlock(&db->lock);
```

Bu durumda `ht_set/ht_get/ht_delete` fonksiyonlarının İÇİNE kilit KOYMA
(çift kilitleme / deadlock olur). Kilit tek sahiplidir: eval seviyesi.

> Alternatif: kilidi ht_* içine koyup compound komutlar için ayrı
> `ht_lock(db)/ht_unlock(db)` API'si açmak. Daha karmaşık; şimdilik gerek yok.

---

## 5. [KİLİT] SİNSİ HATA: `ht_get` ham pointer döndürüyor  — `hash-table.c:37`

```c
return e->value;   // ham pointer — kilidi bıraktıktan sonra kullanmak TEHLİKELİ
```

Kilidi bıraktıktan sonra başka thread `ht_delete`/`ht_set` ile bu `value`'yu
`free()` edebilir → okuyan thread **use-after-free** yaşar.

**Kural:** Dönen değeri **kilit hâlâ elimizdeyken kopyala**, sonra kullan.
Madde 4'teki komut-seviyesi kilit bunu zaten sağlar (get + printf aynı kilit
bloğunda). Sonucu socket'e yazarken de kilit altında bir buffer'a kopyala,
kilidi bırak, sonra `send()` et:

```c
pthread_mutex_lock(&db->lock);
char *v = ht_get(db, key);
char out[512];
int len = v ? snprintf(out, sizeof out, "\"%s\"\n", v)
            : snprintf(out, sizeof out, "(nil)\n");
pthread_mutex_unlock(&db->lock);   // kopyaladık, artık güvenli
send(fd, out, len, 0);             // yavaş IO'yu kilit DIŞINDA yap
```

---

## 6. [ÇIKTI] eval çıktısını stdout yerine socket'e yönlendir  — `parser/eval.c`

Şu an `eval` her yerde `printf`/`puts` ile **stdout**'a yazıyor (eval.c boyunca).
TCP'de çıktı client'ın socket'ine gitmeli. Ayrıca birden çok thread aynı fd'ye
karışık yazmasın diye önce buffer'a topla, sonra tek `send()` at.

Öneri: eval imzasını bir output buffer alacak şekilde değiştir:

```c
// printf(...) yerine:
static int eval(ASTNode *node, HashTable *db, char *out, size_t cap);
// içeride: int len = 0; len += snprintf(out+len, cap-len, "...");
// en sonda handler: send(fd, out, len, 0);
```

Böylece `run()` de (main.c:11) TCP handler da aynı eval'i kullanabilir.

---

## 7. [ATOMIC] `count` alanı — şimdilik gerek YOK

`ht->count++` / `count--` atomik değil (hash-table.c:32,45).
**AMA** madde 4'teki global kilit `count`'u da koruyor, o yüzden şu an
atomic'e ihtiyaç yok.

Sadece ileride kilidi kaldırıp lock-free sayaç istersen:

```c
#include <stdatomic.h>
// struct: atomic_int count;
atomic_fetch_add(&ht->count, 1);   // ht_set
atomic_fetch_sub(&ht->count, 1);   // ht_delete
int n = atomic_load(&ht->count);   // DBSIZE
```

---

## 8. [İLERİ — opsiyonel] Daha fazla paralellik istersen

Global mutex her şeyi seri yapar (Redis gibi — genelde yeterli). Darboğaz olursa:

- **`pthread_rwlock_t`**: okumalar (GET/MGET/EXISTS/KEYS) paralel, yazmalar exclusive.
  Read-heavy yük için iyi. Dikkat: madde 5'teki pointer-kopyalama kuralı yine geçerli.
- **Striped / per-bucket kilitler**: `TABLE_SIZE` yerine ~16 kilitlik dizi,
  `lock[idx % 16]`. Daha çok paralellik ama:
  - `RENAME` iki farklı bucket'a dokunur → **iki kilit** → **deadlock riski**.
    Çözüm: kilitleri her zaman aynı sırada al (örn. adres/indeks sırasına göre).
  - `KEYS`/`DBSIZE`/`FLUSHDB` tüm tabloyu gezer → tüm kilitleri almak gerekir.

Bu yüzden **önce global mutex ile başla**, çalıştığından emin ol, sonra
ölçüp gerçekten gerekiyorsa ince kilitlemeye geç.

---

## 9. [TEST] Doğrulama

- Kilitsiz hali `helgrind` / `ThreadSanitizer` (`-fsanitize=thread`) altında koştur
  → race'leri gösterir.
- Çok sayıda client'la eşzamanlı `SET`/`GET`/`APPEND` bas, `DBSIZE` tutarlı mı bak.
- Aynı key'e paralel `APPEND` at → global kilit yoksa sonuç bozulur, varsa tutarlı.
