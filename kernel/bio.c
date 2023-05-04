// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

//struct {
//  struct spinlock lock;
//  struct buf buf[NBUF];
//
//  // Linked list of all buffers, through prev/next.
//  // Sorted by how recently the buffer was used.
//  // head.next is most recent, head.prev is least.
//  struct buf head;
//} bcache;

#define NBUCKET 13
struct {
    struct spinlock lock[NBUCKET];
    struct buf buf[NBUF];   // 所有的cache块
    struct buf head[NBUCKET];  // 哈希表中每个Bucket的第一个块
} bcache;
int hash (int n) {
    return n%NBUCKET;
}


void
binit(void)
{
  struct buf *b;

  for(int i=0;i<NBUCKET;i++) {
      initlock(&(bcache.lock[i]), "bcache");
  }


  // 初始化将所有块放在第一个bucket
  bcache.head[0].next=&bcache.buf[0];
  for(b = bcache.buf; b < bcache.buf+NBUF-1; b++){
    b->next=b+1;
    initsleeplock(&b->lock, "buffer");
  }
  initsleeplock(&b->lock, "buffer");
}

//辅助函数，用于写cache
void
write_cache(struct buf *take_buf, uint dev, uint blockno)
{
    take_buf->dev = dev;
    take_buf->blockno = blockno;
    take_buf->valid = 0;
    take_buf->refcnt = 1;
    take_buf->time = ticks;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *last;  // last记录hash表的一个bucket中最后一个节点，便于插入新节点
  struct buf *take_buf=0;  // 记录空闲块
  int bucket=hash(blockno);

  acquire(&bcache.lock[bucket]);

  // Is the block already cached?
  for(b = bcache.head[bucket].next, last=&(bcache.head[bucket]); b; b = b->next, last=last->next){
    if(b->dev == dev && b->blockno == blockno){ // 找到了
      b->refcnt++;
      b->time=ticks;
      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
    if(b->refcnt==0) { // 有空闲块，就拿它了
        take_buf=b;
    }
  }

  if(take_buf) {  // 有空闲块,使用它
      write_cache(take_buf, dev, blockno);
      release(&bcache.lock[bucket]);
      acquiresleep(&(take_buf->lock));
      return take_buf;
  }

  // 到其他bucket中寻找最久未使用的空闲块
  int lock_num=-1;  //
  uint time=__UINT32_MAX__;  // 最小的最后一次使用的时间
  struct buf *tmp;
  struct buf *take_buf_prev=0;  // 最后选定的块 的前一个块
  for(int i=0; i<NBUCKET;i++) {
      if(i==bucket) continue;
      acquire(&bcache.lock[i]);  // 获取bucket的锁
      for(b=bcache.head[bucket].next, tmp=&(bcache.head[i]);b;b=b->next, tmp=tmp->next) {
          if(b->refcnt==0) {  // 空闲块
              if(b->time<time) {
                  time = b->time;
                  take_buf_prev = tmp;
                  take_buf = b;

                  // 如果上一个记录的空闲块不在这个bucket中，释放锁
                  if (lock_num != -1 && lock_num != i && holding(&(bcache.lock[lock_num])))
                      release(&bcache.lock[lock_num]);
                  lock_num = i;
              }
          }
      }
      // 没有用到这个bucket中的块，则释放锁
      if(lock_num!=i)
          release(&bcache.lock[i]);
  }
  if(!take_buf) {
      panic("bget: no buffers");
  }

  // 将选中的块从本来的bucket中steal出来
  take_buf_prev->next=take_buf->next;
  take_buf->next=0;
  release(&bcache.lock[lock_num]);
  last->next=take_buf;
  write_cache(take_buf, dev, blockno);

  release(&bcache.lock[bucket]);
  acquiresleep(&(take_buf->lock));

  return take_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket=hash(b->blockno);
  acquire(&bcache.lock[bucket]);
  b->refcnt--;
//  if (b->refcnt == 0) {
//    // no one is waiting for it.
//    b->next->prev = b->prev;
//    b->prev->next = b->next;
//    b->next = bcache.head.next;
//    b->prev = &bcache.head;
//    bcache.head.next->prev = b;
//    bcache.head.next = b;
//  }

  release(&bcache.lock[bucket]);
}

void
bpin(struct buf *b) {
    int bucket= hash(b->blockno);
    acquire(&bcache.lock[bucket]);
    b->refcnt++;
    release(&bcache.lock[bucket]);
}

void
bunpin(struct buf *b) {
    int bucket= hash(b->blockno);
    acquire(&bcache.lock[bucket]);
    b->refcnt--;
    release(&bcache.lock[bucket]);
}


