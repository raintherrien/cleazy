#include <cleazy/common.h>
#include <cleazy/impl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Try to fit block buffers on a single 4KiB page */
#define CLEAZY_TLDBLKBUFSZ  (4096 - sizeof(struct cleazy_blklst *)) / sizeof(struct cleazy_blk)
#define CLEAZY_DSCLSTINITSZ (1 << 10)
_Static_assert(CLEAZY_DSCLSTINITSZ < SIZE_MAX/sizeof(struct cleazy_dsc),
               "Descriptor list size would cause size_t overflow");

/*
 * Intrusive linked list of block chunks attached to a superblock and
 * filled by cleazy_push. Rather than realloc and spend geometric time
 * moving data, simply allocate a new chunk when you run out of space.
 */
struct cleazy_blklst {
    struct cleazy_blklst *next;
    struct cleazy_blk     blks[CLEAZY_TLDBLKBUFSZ];
};

/*
 * Thread local superblock keeps threads from stepping on eachother, but
 * also requires us to call cleazy_thread to initialize and
 * cleazy_flush to coalesce data from all threads.
 */
struct cleazy_sb {
    struct cleazy_sb     *next;       /* for cleazy_tlist linked list */
    struct cleazy_blklst *blklst;
    struct cleazy_blk    *blks;
    const char           *thread_name;
    uint64_t              thread_id;
    /* easy_profiler v2.1.0 file format imposes 2^32 max blocks */
    uint32_t              blks_count;
};
_Thread_local struct cleazy_sb *cleazy_tsb;

/*
 * For cleazy_flush to find the superblock of each thread we create a
 * linked list of superblocks. Each thread registers its superblock here
 * in cleazy_thread.
 */
static struct cleazy_sb * _Atomic cleazy_tlist;

/*
 * Atomic counter to give threads unique IDs.
 * TODO: Could rely on user to pass us a more meaningful identifier.
 */
static atomic_size_t cleazy_tid;

/*
 * Application wide flag to enable/disable profiling. Updated by
 * cleazy_pause and cleazy_resume.
 */
static atomic_bool cleazy_profiling = 1;

/*
 * Geometrically grow the size of our block buffer
 */
static void cleazy_grow_tld_blks(void);

/*
 * If self profiling is enabled we need a descriptor to point at.
 * TODO: Should this be conditionally compiled or always available?
 */
#ifdef CLEAZY_PROFILE_SELF
const struct cleazy_dsc cleazy_dsc_push = {
    .name = "cleazy_push", .file = "self profiling", .line = 0
};
#endif

void
cleazy_cleanup(void)
{
    /* Free threads and block lists */
    struct cleazy_sb *tlist_head = atomic_exchange(&cleazy_tlist, NULL);
    while (tlist_head) {
        struct cleazy_sb *tlist_tail = tlist_head->next;
        struct cleazy_blklst *blklst_head = tlist_head->blklst;
        while (blklst_head) {
            struct cleazy_blklst *blklst_next = blklst_head->next;
            free(blklst_head);
            blklst_head = blklst_next;
        }
        free(tlist_head);
        tlist_head = tlist_tail;
    }
}

/*
 * TODO: easy_profiler doesn't seem to specify an endianness. That
 * gives me the heebie-jeebies. Maybe I've been writing low level code
 * for too long. :) Whatever, just use platform local.
 */
void
cleazy_flush(const char *filename)
{
    /*
     * TODO: Bogus context switch because the easy_profiler gui
     * complains about zero context switches
     */
    const uint32_t ctxswnum = 1;
    const uint16_t ctxswsz = 25;
    const char ctxswbogus[25] = { 0 };

    /*
     * Determine number of blocks, unique descriptors, and required mem.
     * This is all fiddly because we don't want to allocate a central
     * buffer for blocks. We may have GB of data. We're unlikely to have
     * more than a few thousand block descriptors though.
     *
     * blkmem is wonky because it also encompasses each thread header
     * and context switch info.
     */
    size_t dscsz = CLEAZY_DSCLSTINITSZ;
    const struct cleazy_dsc **dsc = malloc(dscsz * sizeof *dsc);
    if (!dsc) goto dsc_alloc_failed;
    uint64_t blkmem = 0;
    uint64_t dscmem = 0;
    uint32_t blknum = 0;
    uint32_t dscnum = 0;
    uint64_t first  = -1;
    uint64_t last   = 0;
    struct cleazy_sb *tsb = cleazy_tlist;
    while (tsb) {
        /* Thread header memory */
        blkmem += /* hard coded thread header length */
                  8 + 2 + 4 + 4 +
                  /* bogus context switch size and single entry */
                  2 + sizeof(ctxswbogus) / sizeof(*ctxswbogus) +
                  strlen(tsb->thread_name);
        /*
         * Iterate over this thread's block linked list and sum up mem
         * and add unique descriptors.
         */
        struct cleazy_blklst *blklst = tsb->blklst;
        uint32_t blks_count = tsb->blks_count;
        while (blklst) {
            blknum += blks_count;
            struct cleazy_blk *blks = blklst->blks;
            for (uint32_t i = 0; i < blks_count; ++ i) {
                struct cleazy_blk *blk = blks + i;
                /* TODO: We don't support runtime block names... Yet. */
                size_t blknameln = 1;
                blkmem += /* hard coded block header length */
                          8 + 8 + 4 + blknameln;
                uint32_t dscid = -1;
                for (uint32_t di = 0; di < dscnum; ++ di) {
                    if (dsc[di] == blk->dsc) {
                        dscid = di;
                        break;
                    }
                }
                if (blk->begin < first) first = blk->begin;
                if (blk->end   > last)  last  = blk->end;
                if (dscid < (uint32_t)-1) {
                    blk->dscid = dscid;
                } else {
                    dscid = dscnum ++;
                    dsc[dscid] = blk->dsc;
                    /* Zero terminated string length */
                    size_t dscnameln  = strlen(blk->dsc->name) + 1;
                    size_t filenameln = strlen(blk->dsc->file) + 1;
                    /* Hard coded descriptor length */
                    size_t size = 4+4+4+1+1+2 + dscnameln + filenameln;
                    if (size > (uint16_t)-1 ||
                        dscmem > ((uint16_t)-1) - size)
                    {
                        perror("Error cleazy descriptor length exceeds 2^16-1");
                        goto failure_needs_free;
                    }
                    dscmem += size;
                    /* grow */
                    if (dscnum >= dscsz) {
                        if (dscsz * sizeof *dsc >= SIZE_MAX / 2) {
                            perror("Error cleazy descriptor size exceeds SIZE_MAX");
                            goto failure_needs_free;
                        }
                        dscsz *= 2;
                        dsc = realloc(dsc, dscsz * sizeof *dsc);
                        if (!dsc) {
                            perror("Error growing cleazy descriptor array");
                            goto failure_needs_free;
                        }
                    }
                    blk->dscid = dscid;
                }
            }
            /* All blocks besides head are full */
            blklst = blklst->next;
            blks_count = CLEAZY_TLDBLKBUFSZ;
        }
        tsb = tsb->next;
    }

    FILE *pf = fopen(filename, "w");
    if (!pf) {
        perror("Error creating/opening cleazy perf file");
        goto failure_needs_free;
    }

    /* Write file header */
    const uint32_t sig = ('E' << 24) | ('a' << 16) | ('s' << 8) | 'y';
    const uint32_t ver = (2 << 24) | (1 << 16);
    const uint64_t pid = 0; /* TODO: Fake process ID */
    const uint64_t frq = 0; /* TODO: What is CPU frequency / ratio? To scale times? */
    fwrite(&sig,    sizeof(sig),    1, pf); /* EasyProfiler signature */
    fwrite(&ver,    sizeof(ver),    1, pf); /* File version */
    fwrite(&pid,    sizeof(pid),    1, pf); /* Profiled PID */
    fwrite(&frq,    sizeof(frq),    1, pf); /* CPU frequency / ratio */
    fwrite(&first,  sizeof(first),  1, pf); /* Begin time */
    fwrite(&last,   sizeof(last),   1, pf); /* End time */
    fwrite(&blkmem, sizeof(blkmem), 1, pf);
    fwrite(&dscmem, sizeof(dscmem), 1, pf);
    fwrite(&blknum, sizeof(blknum), 1, pf);
    fwrite(&dscnum, sizeof(dscnum), 1, pf);
    const uint32_t thrdnum = cleazy_tid;
    const uint32_t bookmarks_and_padding = 0;
    fwrite(&thrdnum, sizeof(thrdnum), 1, pf);
    fwrite(&bookmarks_and_padding, sizeof(bookmarks_and_padding), 1, pf);

    /* Write block descriptors */
    for (uint32_t i = 0; i < dscnum; ++ i) {
        const struct cleazy_dsc *d = dsc[i];
        /*
         * Zero terminated string length. We don't really care about
         * uint16_t overflow as we're adding our own termination char.
         */
        uint16_t dscnameln  = strlen(d->name) + 1;
        uint16_t filenameln = strlen(d->file) + 1;
        /* Hard coded descriptor size */
        uint16_t size = 4+4+4+1+1+2 + dscnameln + filenameln;
        uint8_t type   = 1; /* Hardcoded Block */
        uint8_t status = 1; /* Hardcoded ON */
        fwrite(&size,      sizeof(size), 1, pf);      /* Size */
        fwrite(&i,         sizeof(i), 1, pf);         /* Block ID */
        fwrite(&d->line,   sizeof(d->line), 1, pf);   /* Line number */
        fwrite(&d->argb,   sizeof(d->argb), 1, pf);   /* ARGB color */
        fwrite(&type,      sizeof(type), 1, pf);      /* Block type */
        fwrite(&status,    sizeof(status), 1, pf);    /* Block status */
        fwrite(&dscnameln, sizeof(dscnameln), 1, pf); /* Name length */
        fputs(d->name, pf);
        fputc(0, pf);
        fputs(d->file, pf);
        fputc(0, pf);
    }

    /* Write thread events and blocks */
    tsb = cleazy_tlist;
    while (tsb) {
        /* Thread header */
        /* TODO: Address as thread ID, probably not a great idea */
        _Static_assert(sizeof(tsb) == 8, "");
        fwrite(&tsb, sizeof(tsb), 1, pf);
        /* TODO: Thread name doesn't seem to be null terminated */
        uint16_t tnameln = strlen(tsb->thread_name);
        fwrite(&tnameln, sizeof(tnameln), 1, pf);
        fwrite(tsb->thread_name, 1, tnameln, pf);
        /*
         * TODO: Bogus context switch because the easy_profiler gui
         * complains about zero context switches
         */
        fwrite(&ctxswnum, sizeof(ctxswnum), 1, pf);
        fwrite(&ctxswsz, sizeof(ctxswsz), 1, pf);
        fwrite(ctxswbogus, 1, ctxswsz, pf);
        /* Count blocks */
        uint32_t total_blk_count = tsb->blks_count;
        struct cleazy_blklst *head = tsb->blklst;
        while (head->next) {
            total_blk_count += CLEAZY_TLDBLKBUFSZ;
            head = head->next;
        }
        fwrite(&total_blk_count, sizeof(total_blk_count), 1, pf);
        /* Block data */
        struct cleazy_blklst *blklst = tsb->blklst;
        uint32_t blks_count = tsb->blks_count;
        while (blklst) {
            struct cleazy_blk *blks = blklst->blks;
            for (uint32_t i = 0; i < blks_count; ++ i) {
                const struct cleazy_blk *blk = blks + i;
                uint16_t size  = 8+8+4 + 1; /* hard coded block len */
                uint64_t begin = blk->begin;
                uint64_t end   = blk->end;
                uint32_t blkid = blk->dscid;
                fwrite(&size, sizeof(size), 1, pf);
                fwrite(&begin, sizeof(begin), 1, pf);
                fwrite(&end, sizeof(end), 1, pf);
                fwrite(&blkid, sizeof(blkid), 1, pf);
                fputc(0, pf); /* No runtime block name support */
            }
            /* All blocks besides head are full */
            blklst = blklst->next;
            blks_count = CLEAZY_TLDBLKBUFSZ;
        }
        tsb = tsb->next;
    }

    /*
     * We don't support bookmarks but I think the signature at the head
     * of the section is required.
     */
    fwrite(&sig, sizeof(sig), 1, pf);

    if (fclose(pf) != 0) {
        perror("Error closing cleazy perf file");
    }

failure_needs_free: ;
    /* Free all but first block lists */
    struct cleazy_sb *tlist_head = cleazy_tlist;
    while (tlist_head) {
        struct cleazy_sb *tlist_tail = tlist_head->next;
        struct cleazy_blklst *blklst_head = tlist_head->blklst->next;
        tlist_head->blklst->next = NULL;
        tlist_head->blks = tlist_head->blklst->blks;
        tlist_head->blks_count = 0;
        while (blklst_head) {
            struct cleazy_blklst *blklst_next = blklst_head->next;
            free(blklst_head);
            blklst_head = blklst_next;
        }
        tlist_head = tlist_tail;
    }
    free(dsc);
dsc_alloc_failed:
    return;
}

void
cleazy_push(struct cleazy_blk blk)
{
    if (!cleazy_profiling) return;
    if (cleazy_tsb->blks_count >= CLEAZY_TLDBLKBUFSZ) {
        cleazy_grow_tld_blks();
    }
    cleazy_tsb->blks[cleazy_tsb->blks_count ++] = blk;
}

void
cleazy_thread(const char *thread_name)
{
    cleazy_tsb = malloc(sizeof *cleazy_tsb);
    if (!cleazy_tsb) {
        perror("Error allocating cleazy thread local superblock");
        exit(EXIT_FAILURE);
    }
    cleazy_tsb->blklst = NULL;
    cleazy_tsb->thread_name = thread_name;
    cleazy_tsb->thread_id = cleazy_tid ++;
    cleazy_grow_tld_blks();
    /* build a linked list of thread superblocks */
    cleazy_tsb->next = atomic_exchange(&cleazy_tlist, cleazy_tsb);
}

void
cleazy_pause(void)
{
    cleazy_profiling = 0;
}

void
cleazy_resume(void)
{
    cleazy_profiling = 1;
}

static void
cleazy_grow_tld_blks(void)
{
    struct cleazy_blklst *oldlst = cleazy_tsb->blklst;
    cleazy_tsb->blklst = malloc(sizeof *cleazy_tsb->blklst);
    if (cleazy_tsb->blklst) {
        cleazy_tsb->blks = cleazy_tsb->blklst->blks;
        cleazy_tsb->blklst->next = oldlst;
        cleazy_tsb->blks_count = 0;
    } else {
        /*
         * It wouldn't be hard to simply not profile when we see an out
         * of memory condition, but there really isn't much point.
         */
        perror("Error growing cleazy thread local block buffer");
        exit(EXIT_FAILURE);
    }
}
