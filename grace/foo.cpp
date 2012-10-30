#include <sys/mman.h>
#include <iostream>
using namespace std;

#include <pthread.h>

template <class TYPE>
class localptr {
public:

  localptr()
  {
    pthread_key_create (&key, NULL); 
    pthread_setspecific (key, NULL);
  }

  ~localptr() {
  }

  TYPE operator*() const {
    return *(get());
  }

  localptr& operator=(TYPE * p) {
    set (p);
    return *this;
  }

private:

  TYPE * get() const {
    void * val = pthread_getspecific (key);
    TYPE * tval = (TYPE *) val;
    return tval;
  }

  void set (TYPE * v) {
    // Assign new one.
    pthread_setspecific (key, (void *) v);
  }

  // Disable copying and assignment.
  localptr<TYPE> (const localptr<TYPE>&);
  localptr<TYPE>& operator=(const localptr<TYPE>&);

  pthread_key_t key;
};


class objheader {
public:
  objheader * next;
  size_t size;
};

class objlist {
public:
  objlist()
    : list (NULL)
  {}

  objheader * pop() {
    if (list) {
      objheader * obj = list;
      list = list->next;
      return obj;
    } else {
      return NULL;
    }
  }
  void push (objheader * o) {
    o->next = list;
    list = o;
  }
private:
  objheader * list;
};


class heapo {
  enum { MAX = 64 * 1024 };
  enum { NUMBINS = 64 };
public:
  void * malloc (size_t sz) {
  ALLOC:
    objheader * obj = objs[getBin(sz)].pop();
    if (obj != NULL) {
      return (void *) (obj + 1);
    } else {
      getMore (sz);
      goto ALLOC;
    }
  }
  void free (void * ptr) {
    objheader * obj = reinterpret_cast<objheader *>(ptr) - 1;
    objs[getBin(obj->size)].push (obj);
  }
private:

  void getMore (size_t sz) {
    // go get more memory.
    cout << "sz = " << sz << endl;
    size_t realSz = realSize(sz);
    cout << "realsz = " << realSz << endl;
    // We will get max objects + headers < MAX, or just one object + header.
    int requestCount;
    requestCount = MAX / (realSz + sizeof(objheader));
    if (requestCount == 0) {
      requestCount = 1;
    }
    size_t requestSz = requestCount * (realSz + sizeof(objheader));
    cout << "reqsize = " << requestSz << endl;
    void * ptr = mmap (NULL, requestSz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    cout << "ptr = " << ptr << endl;
    if (ptr != MAP_FAILED) {
      // Carve it up.
      // NB: would be better to do this reap-style.
      objheader * obj = reinterpret_cast<objheader *>(ptr);
      int bin = getBin(realSz);
      int count = requestCount;
      while (count > 0) {
	obj->size = realSz;
	objs[bin].push (obj);
	obj = (objheader *) ((char *) (obj + 1) + realSz);
	count--;
      }  
    } else {
      // Barf.
      // fail. not implemented yet.
    }
  }

  static size_t realSize (size_t sz) {
    // Round up. Stupid, fix.
    while (sz & (sz-1)) {
      sz += 8;
    }
    return sz;
  }
  static int getBin (size_t sz) {
    // Round up then take log_2. Stupid, fix.
    while (sz & (sz-1)) {
      sz += 8;
    }
    int lg2 = 0;
    size_t s = 1;
    while (s < sz) {
      lg2++;
      s <<= 1;
    }
    return lg2;
  }
  objlist objs[NUMBINS];
};


// List of private heaps.
//   main heap:
//   [8,16,...]
//
//   [heap]       [heap]      [heap]
//   [8,16,...]   [8,16,...]  [8,16,...]
//
//   a malloc call could go through all heaps, but only once.
//   malloc(8) -- could get all the heaps.
//   for now, let's do list of objects.


// notes:

//  1 - be adaptive? don't cache more memory than you have ever
//  used. a 64K fixed threshold may not make sense. but if each thread
//  can only hold, say, a multiple of more memory than ever requested
//  up to a certain amount, that would be better. consider this case:
//  T threads, each allocates 8 bytes, but gets 64K. That's 64K/8 = 8K
//  blowup. obviously not asymptotic, of course...as memory allocated
//  grows, this problem goes away.

//  2 - reusing locally-freed memory => "passive false sharing". not
//  ideal.  can we do better?

//  3 - tcmalloc uses prime number triggers for "garbage collection",
//  which works because each thread actually just holds an index to a heap
//  (globally accessible).
// 


template <class SuperHeap>
class buffer {
public:

  // maintain a list of heaps -- move bin with stuff in it to main
  // heap once main heap is empty.

  // multiple allocate = grab N objects (would be nice to just grab a heap)
  //   actually this will work if we always gin up a heap if none are available (just return NULL!),
  //   otherwise free whole bin.
  // multiple free = free bin

  void * malloc (size_t sz) {
    // If there's something in the buffer, use it.
    // Otherwise, get more.
    refillBuffer();
    return NULL;
  }

  void free (void * ptr) {
    // Put freed object into log of freed objects.
    // When full, dump all freed objects.
    dumpObjects();
  }

private:

  void refillBuffer() {
  }

  void dumpObjects() {
  }

  // Private heap.

  // Buffer of allocated objects.
  // Buffer of freed objects.
};

localptr<int> x;

void * foo (void * n) {
  int * ni = (int *) n;
  x = ni;
  sleep(10);
  x = ni;
  cout << *x << endl;
  return NULL;
}

int
main()
{
  heapo h;
  size_t s = 128;
  for (int i = 0; i < 1; i++) {
    void * ptr = h.malloc (s);
    cout << "malloc ptr = " << ptr << endl;
    h.free (ptr);
    s *= 2;
  }
#if 0
  pthread_t t[8];
  for (int i = 0; i < 8; i++) {
    pthread_create (&t[i], NULL, foo, new int (i));
  }
  for (int i = 0; i < 8; i++) {
    pthread_join (t[i], NULL);
  }
#endif
}
