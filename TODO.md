# TODO — Redis Mimarisiyle TCP Sunucu (ykr-cache)

Hedef: Redis'in çalıştığı gibi çalışmak → **tek thread + event loop (epoll), KİLİT YOK**.
Bu dosya, mevcut kodun (hash table + parser + eval) üstüne Redis tarzı bir ağ
katmanını adım adım, örneklerle anlatır.

---

## 0. Mimari özet — Redis nasıl çalışıyor?

Redis tek bir thread'de şu döngüyü döndürür (buna "event loop" / Redis'te `ae` denir):

```
                 ┌─────────────────────────────────────────┐
                 │            EVENT LOOP (tek thread)        │
   client1 ──┐   │  epoll_wait() → hazır olan fd'leri al     │
   client2 ──┼──▶│    fd okunabilir?  → oku + parse + eval   │
   client3 ──┘   │    fd yazılabilir? → cevabı gönder        │
                 │    listen fd?      → yeni client accept   │
                 └─────────────────────────────────────────┘
```

**Temel fikirler:**
1. **I/O multiplexing (epoll):** Binlerce client'ı tek thread'de, sadece
   *hazır olanlarla* ilgilenerek yönetir. Boşta bekleyen client CPU yemez.
2. **Non-blocking socket'ler:** `accept`/`recv`/`send` asla bloke olmaz;
   veri yoksa `EAGAIN` döner, loop devam eder.
3. **Komutlar SERİ çalışır:** Aynı anda iki komut asla hash table'a dokunmaz.
   → **Race condition yok → mutex/atomic YOK.** (Redis'in sırrı budur.)
4. **Her client'ın kendi buffer'ı var:** okunanlar `querybuf`'a birikir,
   cevaplar `outbuf`'a yazılır, socket yazılabilir olunca gönderilir.

> **Neden mutex yok?** Tek thread olduğu için `ht_set`/`ht_get`/`ht_delete`
> hiçbir zaman eşzamanlı çağrılmaz. `hash-table.c`'ye HİÇBİR kilit eklemene
> gerek yok. (Thread-per-connection seçseydik gerekirdi — ama Redis öyle yapmıyor.)

---

## 1. [BUG] `tcp_listen` return değeri eksik — `net/tcp.c:42`

Başarı durumunda `return TCP_SUCCESS;` yok.

```c
int tcp_listen(int sd, int queue)
{
    if (listen(sd, queue) == -1) { LOG(stderr, "Listen error\n"); return TCP_LIST_ERR; }
    return TCP_SUCCESS;   // <-- EKLE
}
```

---

## 2. [NET] Socket'i non-blocking yap — `net/tcp.h/.c`

Event loop'un temeli. Hem listen fd'si hem her client fd'si non-blocking olmalı.

```c
#include <fcntl.h>

int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return TCP_SOPT_ERR;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return TCP_SOPT_ERR;
    return TCP_SUCCESS;
}
```

Ayrıca `SO_REUSEADDR` aç (yeniden başlatınca "address already in use" olmasın —
`set_socket_opt` zaten var, `SOL_SOCKET`/`SO_REUSEADDR` ile çağır).

---

## 3. [NET] Client struct'ı — her bağlantının durumu — `net/server.h` (yeni)

Redis'teki `client` yapısının sadeleştirilmiş hali. Her client kendi
buffer'larını taşır (paylaşımlı state yok → kilit yok).

```c
#define QUERYBUF_CAP  65536
#define OUTBUF_CAP    65536

typedef struct Client
{
    int    fd;
    char   querybuf[QUERYBUF_CAP];   // socket'ten okunan ham veri birikir
    size_t query_len;
    char   outbuf[OUTBUF_CAP];       // gönderilecek cevap birikir
    size_t out_len;                  // outbuf'ta bekleyen byte
    size_t out_sent;                 // outbuf'tan gönderilmiş byte
} Client;
```

İleride Redis gibi büyümek istersen: dinamik buffer (sabit cap yerine),
`argc/argv`, `last_interaction` (timeout), `db` işaretçisi eklersin.

---

## 4. [NET] Event loop iskeleti (epoll) — `net/server.c` (yeni)

Redis'in `ae` event loop'unun özü. **Level-triggered** epoll (Redis de böyle
kullanır — daha basit ve doğru).

```c
#include <sys/epoll.h>

#define MAX_EVENTS 1024

void server_run(int listen_fd, HashTable *db)
{
    int ep = epoll_create1(0);

    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = NULL }; // listen fd
    epoll_ctl(ep, EPOLL_CTL_ADD, listen_fd, &ev);

    struct epoll_event events[MAX_EVENTS];
    for (;;)
    {
        int n = epoll_wait(ep, events, MAX_EVENTS, -1);   // -1 = süresiz bekle
        for (int i = 0; i < n; i++)
        {
            if (events[i].data.ptr == NULL)               // listen fd → yeni client
                accept_clients(ep, listen_fd);
            else
            {
                Client *c = events[i].data.ptr;
                if (events[i].events & (EPOLLHUP | EPOLLERR)) { close_client(ep, c); continue; }
                if (events[i].events & EPOLLIN)  read_from_client(ep, c, db);
                if (events[i].events & EPOLLOUT) write_to_client(ep, c);
            }
        }
    }
}
```

`data.ptr`'e Client işaretçisi koymak, fd → client eşlemesini bedava verir
(ayrı bir map'e gerek yok). Listen fd için `NULL` kullanıyoruz.

---

## 5. [NET] accept — yeni client kabul et

Non-blocking listen fd'de `accept` döngüsü: `EAGAIN` gelene kadar tüm bekleyen
bağlantıları al (level-triggered'da tek seferde birden çok gelebilir).

```c
void accept_clients(int ep, int listen_fd)
{
    for (;;)
    {
        int cfd = accept(listen_fd, NULL, NULL);
        if (cfd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // hepsi alındı
            break;                                                // gerçek hata
        }
        set_nonblocking(cfd);

        Client *c = calloc(1, sizeof(Client));
        c->fd = cfd;

        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = c };
        epoll_ctl(ep, EPOLL_CTL_ADD, cfd, &ev);
    }
}
```

---

## 6. [NET] read + FRAMING — kısmi veriyi doğru işle (KRİTİK)

TCP bir **byte akışıdır**; "SET k v\n" tek `recv`'de gelmeyebilir ("SET k",
sonra " v\n"). Ya da tek `recv`'de birden çok komut gelebilir. Bu yüzden okunanı
`querybuf`'a biriktir, **tam satır (`\n`) oluştukça** işle.

```c
void read_from_client(int ep, Client *c, HashTable *db)
{
    ssize_t n = recv(c->fd, c->querybuf + c->query_len,
                     QUERYBUF_CAP - c->query_len, 0);
    if (n == 0) { close_client(ep, c); return; }              // client kapattı
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;  // veri bitti
        close_client(ep, c); return;
    }
    c->query_len += n;

    // querybuf içindeki her tam satırı işle
    size_t start = 0;
    for (size_t i = 0; i < c->query_len; i++)
    {
        if (c->querybuf[i] == '\n')
        {
            size_t len = i - start;
            char line[512];
            if (len >= sizeof(line)) len = sizeof(line) - 1;
            memcpy(line, c->querybuf + start, len);
            line[len] = '\0';
            if (len && line[len-1] == '\r') line[len-1] = '\0';  // CRLF desteği

            process_command(c, line, db);   // parse + eval → outbuf'a yazar
            start = i + 1;
        }
    }

    // işlenmemiş kalıntıyı (yarım satır) buffer'ın başına kaydır
    memmove(c->querybuf, c->querybuf + start, c->query_len - start);
    c->query_len -= start;

    flush_or_arm_write(ep, c);   // cevabı göndermeyi dene (adım 8)
}
```

---

## 7. [EVAL] Çıktıyı stdout yerine client buffer'ına yönlendir — `parser/eval.c`

Şu an `eval` her yerde `printf`/`puts` ile **stdout**'a yazıyor. Redis modelinde
cevap ilgili client'ın `outbuf`'una gitmeli. `eval` imzasını bir output buffer
alacak şekilde değiştir:

```c
// eval.h
int eval(ASTNode *node, HashTable *db, char *out, size_t cap, size_t *out_len);

// eval.c içinde printf(...) yerine yardımcı:
//   *out_len += snprintf(out + *out_len, cap - *out_len, "...");
// Örn CMD_GET:
//   char *v = ht_get(db, arg(node,0));
//   *out_len += v ? snprintf(out+*out_len, cap-*out_len, "\"%s\"\n", v)
//                 : snprintf(out+*out_len, cap-*out_len, "(nil)\n");
```

`process_command` bunları birbirine bağlar:

```c
void process_command(Client *c, const char *line, HashTable *db)
{
    Parser p; parser_init(&p, line);
    ASTNode *node = parse(&p);
    if (!node) return;                       // parse hatası (mesajı da buffer'a yazdır)
    eval(node, db, c->outbuf, OUTBUF_CAP, &c->out_len);
    free_ast(node); free(node);
}
```

> Not: `main.c`'deki mevcut `run()` de aynı `eval` imzasını kullanacak şekilde
> güncellenir; böylece hem yerel test hem TCP aynı yolu paylaşır.

---

## 8. [NET] write — cevabı gönder (kısmi yazma + EPOLLOUT)

Non-blocking socket'te `send` her şeyi gönderemeyebilir (`EAGAIN` veya kısmi).
Kalanı `EPOLLOUT` ile socket yazılabilir olunca gönder.

```c
void write_to_client(int ep, Client *c)
{
    while (c->out_sent < c->out_len)
    {
        ssize_t n = send(c->fd, c->outbuf + c->out_sent,
                         c->out_len - c->out_sent, 0);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;  // sonra dene
            close_client(ep, c); return;
        }
        c->out_sent += n;
    }
    // hepsi gitti → buffer'ı sıfırla, EPOLLOUT'u kapat
    c->out_len = c->out_sent = 0;
    mod_events(ep, c, EPOLLIN);
}

// read sonrası: hemen göndermeyi dene, kalırsa EPOLLOUT arm et
void flush_or_arm_write(int ep, Client *c)
{
    if (c->out_len == 0) return;
    ssize_t n = send(c->fd, c->outbuf + c->out_sent, c->out_len - c->out_sent, 0);
    if (n > 0) c->out_sent += n;
    if (c->out_sent < c->out_len)
        mod_events(ep, c, EPOLLIN | EPOLLOUT);   // kalanı writable olunca gönder
    else
        c->out_len = c->out_sent = 0;
}
```

`mod_events` = `epoll_ctl(ep, EPOLL_CTL_MOD, c->fd, &ev)` sarmalayıcısı.

---

## 9. [NET] Client kapatma / temizlik

```c
void close_client(int ep, Client *c)
{
    epoll_ctl(ep, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    free(c);
}
```

CMD_EXIT/QUIT gelirse: cevabı yazdıktan sonra `close_client` çağır (Redis'te de
`QUIT` bağlantıyı kapatır).

---

## 10. [MAIN] Sunucuyu ayağa kaldır — `main.c`

```c
int main(void)
{
    HashTable *db = ht_create();
    int fd = create_tcp_socketd(AF_INET);
    int one = 1; int opt[2] = { SOL_SOCKET, SO_REUSEADDR };
    set_socket_opt(fd, opt, &one);
    tcp_bind(fd, 6379);          // Redis portu (istersen değiştir)
    tcp_listen(fd, 512);
    set_nonblocking(fd);
    server_run(fd, db);          // sonsuz event loop
    ht_destroy(db);
    return 0;
}
```

Test: `redis-cli -p 6379` VEYA `nc localhost 6379` → `SET k v` yaz, `GET k` al.

---

## 11. [OPSİYONEL] Redis'e daha çok yaklaşmak istersen

Sırayla, ihtiyaca göre:

- **RESP protokolü:** Redis'in gerçek wire formatı (`*3\r\n$3\r\nSET\r\n...`).
  `redis-cli` bunu konuşur. Şu anki satır-bazlı (inline) protokol de `redis-cli`
  ve `nc` ile çalışır; RESP'i sonra ekleyebilirsin. Redis ikisini de destekler.
- **Dinamik buffer:** sabit `QUERYBUF_CAP`/`OUTBUF_CAP` yerine büyüyen buffer
  (büyük değerler/pipeline için).
- **TTL / expire:** anahtara son kullanma zamanı + tembel (lazy) ve aktif
  temizleme. `Date.now` yerine `time(NULL)` kullan.
- **Timer/cron:** `epoll_wait` timeout'unu kullanarak periyodik iş (Redis'in
  `serverCron`'u): expire taraması, istatistik.
- **Kalıcılık:** RDB (snapshot) / AOF (append-only log). Redis bunları
  arka planda `fork()` ile yapar → ana thread bloke olmaz.
- **Pipelining:** zaten framing doğru yapıldıysa bedava gelir (tek `recv`'de
  çok komut → hepsi sırayla işlenir).

---

## 12. Redis'te var AMA bu projede ŞİMDİLİK atlanacaklar

- Çok thread'li komut çalıştırma (Redis de yapmıyor — sadece I/O thread'leri var).
- Cluster / replication / pub-sub.
- Lua scripting, transactions (MULTI/EXEC).

Bunlara ihtiyaç yok; **tek thread + epoll** çekirdeği bittiğinde çalışan bir
in-memory cache'in olur.

---

## 13. Test / doğrulama

- `nc localhost 6379` ile elle: `SET a 1`, `GET a`, `APPEND a x`, `DEL a`.
- Aynı anda birkaç `nc` bağla → hepsi bağımsız cevap almalı (event loop testi).
- Yarım komut gönder (`printf 'SET k'` newline'sız) → sunucu beklemeli,
  `\n` gelince işlemeli (framing testi).
- Tek satırda pipeline: `printf 'SET a 1\nGET a\n' | nc localhost 6379`
  → iki cevap sırayla gelmeli.
- Valgrind ile client bağlan/kapat döngüsü → memory leak kontrolü.

---

## Uygulama sırası (özet checklist)

1. [ ] `tcp_listen` return fix (Adım 1)
2. [ ] `set_nonblocking` + `SO_REUSEADDR` (Adım 2)
3. [ ] `Client` struct (Adım 3)
4. [ ] epoll event loop iskeleti (Adım 4)
5. [ ] accept handler (Adım 5)
6. [ ] read + framing (Adım 6)
7. [ ] `eval` çıktısını buffer'a taşı (Adım 7)
8. [ ] write + EPOLLOUT (Adım 8)
9. [ ] close_client (Adım 9)
10. [ ] `main` → server_run (Adım 10)
11. [ ] `net/server.c`'yi `CMakeLists.txt`'teki `add_executable`'a ekle
```
