#include "evs_internal.h"

static void pseudo_encryptor(char *buf, size_t s, const char *key, size_t );


/* This is a dumbest encryption you have ever seen. */
static void
pseudo_encryptor(char *buf, size_t s, const char *key, size_t key_s)
{
  size_t i;
  char *dst = buf;

  if (dst != NULL)
    {
      while(--s)
	{
	  for (i =0; i < key_s; i++)
	    {
	      dst[s] = buf[s] ^ key[i]; /* xor operation */
	  
	      *buf = *dst; /* copy to original */
	    }
	}
    }
}

void encryptor(char *buf, size_t s, const char *key, size_t key_s)
{
  pseudo_encryptor(buf, s, key, key_s);
}
