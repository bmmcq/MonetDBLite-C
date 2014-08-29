/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2014 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * (c) M. L. Kersten, P. Boncz, S. Manegold, N. Nes, K.S. Mullender
 * Common BAT Operations
 * We factor out all possible overhead by inlining code.  This
 * includes the macros BUNhead and BUNtail, which do a test to see
 * whether the atom resides in the buns or in a variable storage
 * heap. The updateloop(dstbat, srcbat, operation) macro invokes
 * operation(dstbat, BUNhead(srcbat), BUNtail(srcbat)) on all buns of
 * the srcbat, but testing only once where they reside.
 */
#include "monetdb_config.h"
#include "gdk.h"
#include "gdk_private.h"

#define updateloop(bn, b, func)						\
	do {								\
		BATiter bi = bat_iterator(b);				\
		BUN p1, p2;						\
									\
		BATloop(b, p1, p2) {					\
			func(bn, BUNhead(bi, p1), BUNtail(bi, p1));	\
		}							\
	} while (0)

/*
 * BAT insert/delete/replace
 * The content of a BAT can be appended to (removed from) another
 * using BATins (BATdel).
 */
static BAT *
insert_string_bat(BAT *b, BAT *n, int append, int force)
{
	BATiter ni;		/* iterator */
	int tt;			/* tail type */
	size_t toff = ~(size_t) 0;	/* tail offset */
	BUN p, q;		/* loop variables */
	oid o = 0;		/* in case we're appending */
	ptr hp, tp;		/* head and tail value pointers */
	unsigned char tbv;	/* tail value-as-bte */
	const unsigned char *tbp = NULL;	/* tail value-as-bte */
	unsigned short tsv;	/* tail value-as-sht */
	const unsigned short *tsp = NULL;	/* tail value-as-sht */
#if SIZEOF_VAR_T == 8
	unsigned int tiv;	/* tail value-as-int */
	const unsigned int *tip = NULL;	/* tail value-as-int */
#endif
	var_t v;		/* value */
	const var_t *tvp = NULL;	/* value */
	int ntw, btw;		/* shortcuts for {b,n}->t->width */

	assert(b->H->type == TYPE_void || b->H->type == TYPE_oid);
	if (n->batCount == 0)
		return b;
	ni = bat_iterator(n);
	btw = b->T->width;
	ntw = n->T->width;
	hp = NULL;
	tp = NULL;
	if (b->H->type != TYPE_void) {
		hp = &o;
		o = MAXoid(b);
	}
	tt = b->ttype;
	if (tt == TYPE_str &&
	    (!GDK_ELIMDOUBLES(b->T->vheap) || b->batCount == 0) &&
	    !GDK_ELIMDOUBLES(n->T->vheap) &&
	    b->T->vheap->hashash == n->T->vheap->hashash &&
	    /* if needs to be kept unique, take slow path */
	    (b->tkey & BOUND2BTRUE) == 0 &&
	    /* if view, only copy if significant part of parent is used */
	    (VIEWtparent(n) == 0 ||
	     BATcapacity(BBP_cache(VIEWtparent(n))) < 2 * BATcount(n))) {
		/* append string heaps */
		toff = b->batCount == 0 ? 0 : b->T->vheap->free;
		/* make sure we get alignment right */
		toff = (toff + GDK_VARALIGN - 1) & ~(GDK_VARALIGN - 1);
		assert(((toff >> GDK_VARSHIFT) << GDK_VARSHIFT) == toff);
		/* if in "force" mode, the heap may be shared when
		 * memory mapped */
		if (HEAPextend(b->T->vheap, toff + n->T->vheap->size, force) < 0) {
			toff = ~(size_t) 0;
			goto bunins_failed;
		}
		memcpy(b->T->vheap->base + toff, n->T->vheap->base, n->T->vheap->size);
		b->T->vheap->free = toff + n->T->vheap->free;
		/* flush double-elimination hash table */
		memset(b->T->vheap->base, 0, GDK_STRHASHSIZE);
		if (b->T->width < SIZEOF_VAR_T &&
		    ((size_t) 1 << 8 * b->T->width) < (b->T->width <= 2 ? (b->T->vheap->size >> GDK_VARSHIFT) - GDK_VAROFFSET : (b->T->vheap->size >> GDK_VARSHIFT))) {
			/* offsets aren't going to fit */
			if (GDKupgradevarheap(b->T, (var_t) (b->T->vheap->size >> GDK_VARSHIFT), 0, force) == GDK_FAIL) {
				toff = ~(size_t) 0;
				goto bunins_failed;
			}
			btw = b->T->width;
		}
		switch (btw) {
		case 1:
			tt = TYPE_bte;
			break;
		case 2:
			tt = TYPE_sht;
			break;
#if SIZEOF_VAR_T == 8
		case 4:
			tt = TYPE_int;
			break;
#endif
		default:
			tt = TYPE_var;
			break;
		}
		tbp = (const unsigned char *) Tloc(n, BUNfirst(n));
		tsp = (const unsigned short *) Tloc(n, BUNfirst(n));
#if SIZEOF_VAR_T == 8
		tip = (const unsigned int *) Tloc(n, BUNfirst(n));
#endif
		tvp = (const var_t *) Tloc(n, BUNfirst(n));
		b->T->varsized = 0;
		b->T->type = tt;
	}

	BATloop(n, p, q) {
		if (!append)
			hp = b->H->type ? BUNhloc(ni, p) : NULL;
		if (!append && b->H->type && !n->H->type) {
			o = n->H->seq;
			hp = &o;
		}

		if (toff != ~(size_t) 0) {
			switch (ntw) {
			case 1:
				v = (var_t) *tbp + GDK_VAROFFSET;
				tbp++;
				break;
			case 2:
				v = (var_t) *tsp + GDK_VAROFFSET;
				tsp++;
				break;
#if SIZEOF_VAR_T == 8
			case 4:
				v = (var_t) *tip;
				tip++;
				break;
#endif
			default:
				v = *tvp;
				tvp++;
				break;
			}
			v = (var_t) ((((size_t) v << GDK_VARSHIFT) + toff) >> GDK_VARSHIFT);
			assert(v >= GDK_VAROFFSET);
			assert(((size_t) v << GDK_VARSHIFT) < b->T->vheap->free);
			switch (btw) {
			case 1:
				assert(v - GDK_VAROFFSET < ((var_t) 1 << 8));
				tbv = (unsigned char) (v - GDK_VAROFFSET);
				tp = (ptr) &tbv;
				break;
			case 2:
				assert(v - GDK_VAROFFSET < ((var_t) 1 << 16));
				tsv = (unsigned short) (v - GDK_VAROFFSET);
				tp = (ptr) &tsv;
				break;
#if SIZEOF_VAR_T == 8
			case 4:
				assert(v < ((var_t) 1 << 32));
				tiv = (unsigned int) v;
				tp = (ptr) &tiv;
				break;
#endif
			default:
				tp = (ptr) &v;
				break;
			}
		} else {
			tp = b->T->type ? BUNtail(ni, p) : NULL;
			assert(tp != NULL);
		}
		bunfastins(b, hp, tp);
		if (append)
			o++;
	}
	if (toff != ~(size_t) 0) {
		b->T->varsized = 1;
		b->T->type = TYPE_str;
	}
	return b;
      bunins_failed:
	if (toff != ~(size_t) 0) {
		b->T->varsized = 1;
		b->T->type = TYPE_str;
	}
	return NULL;
}

#define bunins(b,h,t) if (BUNins(b,h,t,force) == NULL) return NULL;
BAT *
BATins(BAT *b, BAT *n, bit force)
{
	BAT *tmp = NULL, *res = NULL;
	int fastpath = 0;
	int countonly;

	if (b == NULL || n == NULL || BATcount(n) == 0) {
		return b;
	}
	if (b->htype != TYPE_void && b->htype != TYPE_oid) {
		GDKerror("BATins: input must be (V)OID headed\n");
		return NULL;
	}
	ALIGNins(b, "BATins", force);
	BATcompatible(b, n);

	countonly = (b->htype == TYPE_void && b->ttype == TYPE_void);

	if (BUNlast(b) + BATcount(n) > BUN_MAX) {
		GDKerror("BATins: combined BATs too large\n");
		return NULL;
	}

	if (b->htype != TYPE_void &&
	    (b->ttype == TYPE_void ||
	     (!b->H->hash && b->T->hash &&
	      ATOMstorage(b->ttype) == TYPE_int))) {	/* OIDDEPEND */
		return BATmirror(BATins(BATmirror(b), BATmirror(n), force));
	}

	if (BUNlast(b) + BATcount(n) > BATcapacity(b)) {
		/* if needed space exceeds a normal growth extend just
		 * with what's needed */
		BUN ncap = BUNlast(b) + BATcount(n);
		BUN grows = BATgrows(b);

		if (ncap > grows)
			grows = ncap;
		if (BATextend(b, grows) == NULL)
			goto bunins_failed;
	}

	if (b->htype == TYPE_void && b->hseqbase != oid_nil) {
		BATiter ni = bat_iterator(n);
		BUN t = (BUN) b->hseqbase + BATcount(b) - 1;
		oid h = *(oid *) BUNhead(ni, BUNfirst(n));

		if (BATcount(b) == 0 && (BATcount(n) == 1 || BAThdense(n))) {
			b->hseqbase = h;
		} else if (t + 1 != (BUN) h || !BAThdense(n)) {
			b = BATmaterializeh(b);
			countonly = 0;
			if (b == NULL)
				return NULL;
		}
	}
	if (b->ttype == TYPE_void && b->tseqbase != oid_nil) {
		BATiter ni = bat_iterator(n);
		BUN t = (BUN) b->tseqbase + BATcount(b) - 1;
		oid h = *(oid *) BUNtail(ni, BUNfirst(n));

		if (BATcount(b) == 0 && (BATcount(n) == 1 || BATtdense(n))) {
			b->tseqbase = h;
		} else if (t + 1 != (BUN) h || !BATtdense(n)) {
			b = BATmaterializet(b);
			countonly = 0;
			if (b == NULL)
				return NULL;
		}
	}
	if (b->T->hash == NULL &&
	    (b->tkey & BOUND2BTRUE) == 0 &&
	    ((b->hkey & BOUND2BTRUE) == 0 || n->hkey) &&
	    (b->H->hash == NULL || ATOMstorage(b->htype) == ATOMstorage(TYPE_oid))) {
		if (b->hkey & BOUND2BTRUE && b->batCount > 0) {
			tmp = n = BATkdiff(n, b);
			if (n == NULL)
				return NULL;
		}
		fastpath = 1;
	}

	if (b->H->hash == NULL && b->hkey & BOUND2BTRUE &&
	    (b->batCount > 0 || !n->hkey) &&
	    (BATcount(b) ||
	     (BATcount(n) && n->htype != TYPE_void && !n->hdense))) {
		BAThash(b, BATcount(b) + BATcount(n));
	}
	if (b->T->hash == NULL && b->tkey & BOUND2BTRUE &&
	    (b->batCount > 0 || !n->tkey) &&
	    (BATcount(b) ||
	     (BATcount(n) && n->ttype != TYPE_void && !n->tdense))) {
		BAThash(BATmirror(b), BATcount(b) + BATcount(n));
	}
	b->batDirty = 1;
	if (fastpath) {
		BUN p, q, r = BUNlast(b);

		if (BATcount(b) == 0) {
			ALIGNset(b, n);
		} else if (BATcount(n)) {
			BUN last = BUNlast(b) - 1;
			BATiter ni = bat_iterator(n);
			BATiter bi = bat_iterator(b);
			int xx = ATOMcmp(b->htype, BUNhead(ni, BUNfirst(n)), BUNhead(bi, last));
			if (BAThordered(b) && (!BAThordered(n) || xx < 0)) {
				b->hsorted = FALSE;
				b->H->nosorted = r;
				if (b->hdense) {
					b->hdense = FALSE;
					b->H->nodense = r;
				}
			}
			if (BAThrevordered(b) &&
			    (!BAThrevordered(n) || xx > 0)) {
				b->hrevsorted = FALSE;
				b->H->norevsorted = r;
			}
			if (b->hkey && (b->hkey & BOUND2BTRUE) == 0 &&
			    (!(BAThordered(b) || BAThrevordered(b)) ||
			     n->hkey == 0 || xx == 0)) {
				BATkey(b, FALSE);
			}
			if (b->htype != TYPE_void && b->hsorted && b->hdense &&
			    (BAThdense(n) == 0 ||
			     *(oid *) BUNhloc(bi, last) != 1 + *(oid *) BUNhead(ni, BUNfirst(n)))) {
				b->hdense = FALSE;
				b->H->nodense = r;
			}

			xx = ATOMcmp(b->ttype, BUNtail(ni, BUNfirst(n)), BUNtail(bi, last));
			if (BATtordered(b) && (!BATtordered(n) || xx < 0)) {
				b->tsorted = FALSE;
				b->T->nosorted = r;
				if (b->tdense) {
					b->tdense = FALSE;
					b->T->nodense = r;
				}
			}
			if (BATtrevordered(b) &&
			    (!BATtrevordered(n) || xx > 0)) {
				b->trevsorted = FALSE;
				b->T->norevsorted = r;
			}
			if (b->tkey && (b->tkey & BOUND2BTRUE) == 0 &&
			    (!(BATtordered(b) || BATtrevordered(b)) ||
			     n->tkey == 0 || xx == 0)) {
				BATkey(BATmirror(b), FALSE);
			}
			if (b->ttype != TYPE_void && b->tsorted && b->tdense &&
			    (BATtdense(n) == 0 ||
			     *(oid *) BUNtloc(bi, last) != 1 + *(oid *) BUNtail(ni, BUNfirst(n)))) {
				b->tdense = FALSE;
				b->T->nodense = r;
			}
		}
		if (countonly) {
			BATsetcount(b, b->batCount + n->batCount);
		} else if ((b->htype == TYPE_str &&
			    (b->batCount == 0 || !GDK_ELIMDOUBLES(b->H->vheap)) &&
			    !GDK_ELIMDOUBLES(n->H->vheap) &&
			    b->H->vheap->hashash == n->H->vheap->hashash &&
			    VIEWhparent(n) == 0) ||
			   (b->ttype == TYPE_str &&
			    (b->batCount == 0 || !GDK_ELIMDOUBLES(b->T->vheap)) &&
			    !GDK_ELIMDOUBLES(n->T->vheap) &&
			    b->T->vheap->hashash == n->T->vheap->hashash &&
			    VIEWtparent(n) == 0)) {
			b = insert_string_bat(b, n, 0, force);
		} else if (b->htype == TYPE_void) {
			if (!ATOMvarsized(b->ttype) &&
			    BATatoms[b->ttype].atomFix == NULL &&
			    n->ttype != TYPE_void) {
				/* use fast memcpy if we can */
				memcpy(Tloc(b, BUNlast(b)),
				       Tloc(n, BUNfirst(n)),
				       BATcount(n) * Tsize(n));
				BATsetcount(b, BATcount(b) + BATcount(n));
			} else {
				BATiter ni = bat_iterator(n);

				BATloop(n, p, q) {
					bunfastapp_nocheck(b, r, BUNtail(ni, p), Tsize(b));
					r++;
				}
			}
		} else if (b->H->hash) {
			BUN i = BUNlast(b);
			BATiter ni = bat_iterator(n);

			BATloop(n, p, q) {
				const void *v = BUNhead(ni, p);

				bunfastins_nocheck(b, r, v, BUNtail(ni, p), Hsize(b), Tsize(b));
				HASHins_oid(b->H->hash, i, v);
				r++;
				i++;
			}
		} else if (!ATOMvarsized(b->htype) &&
			   BATatoms[b->htype].atomFix == NULL &&
			   !ATOMvarsized(b->ttype) &&
			   BATatoms[b->ttype].atomFix == NULL &&
			   n->htype != TYPE_void &&
			   n->ttype != TYPE_void) {
			/* use fast memcpy if we can */
			memcpy(Hloc(b, BUNlast(b)),
			       Hloc(n, BUNfirst(n)),
			       BATcount(n) * Hsize(n));
			memcpy(Tloc(b, BUNlast(b)),
			       Tloc(n, BUNfirst(n)),
			       BATcount(n) * Tsize(n));
			BATsetcount(b, BATcount(b) + BATcount(n));
		} else {
			BATiter ni = bat_iterator(n);

			BATloop(n, p, q) {
				bunfastins_nocheck(b, r, BUNhead(ni, p), BUNtail(ni, p), Hsize(b), Tsize(b));
				r++;
			}
		}
		b->H->nonil &= n->H->nonil;
		b->T->nonil &= n->T->nonil;
	} else {
		updateloop(b, n, bunins);
	}
	res = b;
      bunins_failed:
	if (tmp)
		BBPreclaim(tmp);
	return res;
}

BAT *
BATappend(BAT *b, BAT *n, bit force)
{
	BUN sz;
	int fastpath = 1;

	if (b == NULL || n == NULL || (sz = BATcount(n)) == 0) {
		return b;
	}
	assert(!isVIEW(b));
	if (b->htype != TYPE_void && b->htype != TYPE_oid) {
		GDKerror("BATappend: input must be (V)OID headed\n");
		return NULL;
	}
	ALIGNapp(b, "BATappend", force);
	BATcompatible(b, n);

	if (BUNlast(b) + BATcount(n) > BUN_MAX) {
		GDKerror("BATappend: combined BATs too large\n");
		return NULL;
	}

	b->batDirty = 1;

	if (sz > BATcapacity(b) - BUNlast(b)) {
		/* if needed space exceeds a normal growth extend just
		 * with what's needed */
		BUN ncap = BUNlast(b) + sz;
		BUN grows = BATgrows(b);

		if (ncap > grows)
			grows = ncap;
		if (BATextend(b, grows) == NULL)
			goto bunins_failed;
	}

	/* append two void,void bats */
	if (b->ttype == TYPE_void && BATtdense(b)) {
		oid f = n->tseqbase;

		if (n->ttype != TYPE_void)
			f = *(oid *) BUNtloc(bat_iterator(n), BUNfirst(n));

		if (BATcount(b) == 0 && f != oid_nil)
			BATseqbase(BATmirror(b), f);
		if (BATtdense(n) && BATcount(b) + b->tseqbase == f) {
			if (b->htype != TYPE_void) {
				BUN r = BUNlast(b);
				oid m;

				f = MAXoid(b);
				f++;
				if (f + sz >= GDK_oid_max) {
					GDKerror("BATappend: overflow of head value\n");
					return 0;
				}
				m = (oid) (f + sz);
				for (; f < m; f++, r++) {
					bunfastins_nocheck(b, r, (ptr) &f, NULL, Hsize(b), 0);
				}
			} else {
				sz += BATcount(b);
				BATsetcount(b, sz);
			}
			return b;
		}
		/* we need to materialize the tail */
		b = BATmaterializet(b);
		if (b == NULL)
			return NULL;
	}

	IMPSdestroy(b);		/* imprints do not support updates yet */
	/* a hash is useless for void bats */
	if (b->H->hash)
		HASHremove(b);

	if (b->T->hash && (2 * b->T->hash->mask) < (BATcount(b) + sz)) {
		HASHremove(BATmirror(b));
	}
	if (b->T->hash != NULL ||
	    (b->tkey & BOUND2BTRUE) != 0 ||
	    (b->H->hash != NULL && ATOMstorage(b->htype) != ATOMstorage(TYPE_oid)))
		fastpath = 0;

	if (fastpath) {
		BUN p, q, r = BUNlast(b);

		if (BATcount(b) == 0) {
			BATiter ni = bat_iterator(n);

			ALIGNsetH(BATmirror(b), BATmirror(n));
			b->tseqbase = n->tseqbase;

			if (n->tdense && n->ttype == TYPE_oid) {
				b->tseqbase = *(oid *) BUNtail(ni, BUNfirst(n));
			}
			b->tdense = n->tdense;
			b->T->nodense = n->T->nodense;
			b->tkey |= (n->tkey & TRUE);
			b->T->nokey[0] = n->T->nokey[0];
			b->T->nokey[1] = n->T->nokey[1];
			b->T->nonil = n->T->nonil;
		} else {
			BUN last = BUNlast(b) - 1;
			BATiter ni = bat_iterator(n);
			BATiter bi = bat_iterator(b);
			int xx = ATOMcmp(b->ttype, BUNtail(ni, BUNfirst(n)), BUNtail(bi, last));
			if (BATtordered(b) && (!BATtordered(n) || xx < 0)) {
				b->tsorted = FALSE;
				b->T->nosorted = r;
				if (b->tdense) {
					b->tdense = FALSE;
					b->T->nodense = r;
				}
			}
			if (BATtrevordered(b) &&
			    (!BATtrevordered(n) || xx > 0)) {
				b->trevsorted = FALSE;
				b->T->norevsorted = r;
			}
			if (b->tkey &&
			    (!(BATtordered(b) || BATtrevordered(b)) ||
			     n->tkey == 0 || xx == 0)) {
				BATkey(BATmirror(b), FALSE);
			}
			if (b->ttype != TYPE_void && b->tsorted && b->tdense &&
			    (BATtdense(n) == 0 ||
			     *(oid *) BUNtloc(bi, last) != 1 + *(oid *) BUNtail(ni, BUNfirst(n)))) {
				b->tdense = FALSE;
				b->T->nodense = r;
			}
		}
		if (b->ttype == TYPE_str &&
		    (b->batCount == 0 || !GDK_ELIMDOUBLES(b->T->vheap)) &&
		    !GDK_ELIMDOUBLES(n->T->vheap) &&
		    b->T->vheap->hashash == n->T->vheap->hashash &&
		    VIEWtparent(n) == 0) {
			b = insert_string_bat(b, n, 1, force);
			if (b == NULL)
				return NULL;
		} else if (b->htype == TYPE_void) {
			if (!ATOMvarsized(b->ttype) &&
			    BATatoms[b->ttype].atomFix == NULL &&
			    b->ttype != TYPE_void && n->ttype != TYPE_void) {
				/* use fast memcpy if we can */
				memcpy(Tloc(b, BUNlast(b)),
				       Tloc(n, BUNfirst(n)),
				       BATcount(n) * Tsize(n));
				BATsetcount(b, BATcount(b) + BATcount(n));
			} else {
				BATiter ni = bat_iterator(n);

				BATloop(n, p, q) {
					bunfastapp_nocheck(b, r, BUNtail(ni, p), Tsize(b));
					r++;
				}
			}
			if (b->hseqbase != oid_nil)
				b->hrevsorted = 0;
		} else {
			oid o = MAXoid(b);
			BATiter ni = bat_iterator(n);

			o++;
			BATloop(n, p, q) {
				bunfastins_nocheck(b, r, &o, BUNtail(ni, p), Hsize(b), Tsize(b));
				o++;
				r++;
			}
			b->hrevsorted = 0;
		}
	} else {
		BUN p, q;
		BUN i = BUNlast(b);
		BAT *bm = BBP_cache(-b->batCacheid);
		BATiter ni = bat_iterator(n);

		if (b->tkey & BOUND2BTRUE) {
			b->tdense = b->tsorted = b->trevsorted = 0;
			if (b->hseqbase != oid_nil) {
				oid h = b->hseqbase + i;

				BATloop(n, p, q) {
					const void *t = BUNtail(ni, p);

					if (BUNfnd(bm, t) == BUN_NONE) {
						bunfastins(b, &h, t);
						if (b->T->hash) {
							HASHins(bm, i, t);
						}
						h++;
						i++;
					}
				}
				b->hrevsorted = 0;
			} else {
				oid on = oid_nil;

				BATloop(n, p, q) {
					const void *t = BUNtail(ni, p);

					if (BUNfnd(bm, t) == BUN_NONE) {
						bunfastins(b, &on, t);
						if (b->T->hash) {
							HASHins(bm, i, t);
						}
						i++;
					}
				}
				BATkey(b, FALSE);
				b->hdense = b->hsorted = b->hrevsorted = 0;
			}
		} else if (b->hseqbase != oid_nil) {
			oid h;

			if (b->hseqbase + BATcount(b) + BATcount(n) >= GDK_oid_max) {
				GDKerror("BATappend: overflow of head value\n");
				return NULL;
			}
			h = (oid) (b->hseqbase + BATcount(b));

			BATloop(n, p, q) {
				const void *t = BUNtail(ni, p);

				bunfastins(b, &h, t);
				if (b->T->hash) {
					HASHins(bm, i, t);
				}
				i++;
				h++;
			}
			BATkey(BATmirror(b), FALSE);
			b->tdense = b->tsorted = b->trevsorted = 0;
			b->hrevsorted = 0;
		} else {
			oid on = oid_nil;

			BATloop(n, p, q) {
				const void *t = BUNtail(ni, p);

				bunfastins(b, &on, t);
				if (b->T->hash) {
					HASHins(bm, i, t);
				}
				i++;
			}
			BATkey(b, FALSE);
			BATkey(BATmirror(b), FALSE);
			b->hdense = b->hsorted = b->hrevsorted = 0;
			b->tdense = b->tsorted = b->trevsorted = 0;
		}
	}
	b->H->nonil &= n->H->nonil;
	b->T->nonil &= n->T->nonil;
	return b;
      bunins_failed:
	return NULL;
}


#define bundel(b,h,t) do { if (BUNdel(b,h,t,force) == NULL) { GDKerror("BATdel: BUN does not occur.\n"); return NULL; } } while (0)
BAT *
BATdel(BAT *b, BAT *n, bit force)
{
	ERRORcheck(b == NULL, "set:BAT required\n");
	ERRORcheck(n == NULL, "set:BAT required\n");
	if (BATcount(n) == 0) {
		return b;
	}
	ALIGNdel(b, "BATdel", force);
	TYPEcheck(b->htype, n->htype);
	TYPEcheck(b->ttype, n->ttype);
	updateloop(b, n, bundel);
	return b;
}

#define bundelhead(b,h,t) do { if (BUNdelHead(b,h,force) == NULL) return NULL; } while (0)
BAT *
BATdelHead(BAT *b, BAT *n, bit force)
{
	ERRORcheck(b == NULL, "set:BAT required\n");
	ERRORcheck(n == NULL, "set:BAT required\n");
	if (BATcount(n) == 0) {
		return b;
	}
	ALIGNdel(b, "BATdelHead", force);
	TYPEcheck(b->htype, n->htype);
	updateloop(b, n, bundelhead);
	return b;
}

/*
 * The last in this series is a BATreplace, which replaces all the
 * buns mentioned.
 */
#define BUNreplace_force(a,b,c) BUNreplace(a,b,c,force)
BAT *
BATreplace(BAT *b, BAT *n, bit force)
{
	if (b == NULL || n == NULL || BATcount(n) == 0) {
		return b;
	}
	BATcompatible(b, n);
	updateloop(b, n, BUNreplace_force);

	return b;
}


/*
 *  BAT Selections
 * The BAT selectors are among the most heavily used operators.
 * Their efficient implementation is therefore mandatory.
 *
 * The interface supports seven operations: BATslice, BATselect,
 * BATfragment, BATproject, BATrestrict.
 *
 * BAT slice
 * This function returns a horizontal slice from a BAT. It optimizes
 * execution by avoiding to copy when the BAT is memory mapped (in
 * this case, an independent submap is created) or else when it is
 * read-only, then a VIEW bat is created as a result.
 *
 * If a new copy has to be created, this function takes care to
 * preserve void-columns (in this case, the seqbase has to be
 * recomputed in the result).
 *
 * Note that the BATslice() is used indirectly as well as a special
 * case for BATselect (range selection on sorted column) and
 * BATsemijoin (when two dense columns are semijoined).
 *
 * NOTE new semantics, the selected range is excluding the high value.
 */
#undef BATslice
BAT *
BATslice(BAT *b, BUN l, BUN h)
{
	BUN low = l;
	BAT *bn;
	BATiter bni, bi = bat_iterator(b);
	oid foid;		/* first oid value if oid column */

	BATcheck(b, "BATslice");
	if (h > BATcount(b))
		h = BATcount(b);
	if (h < l)
		h = l;
	l += BUNfirst(b);
	h += BUNfirst(b);

	if (l > BUN_MAX || h > BUN_MAX) {
		GDKerror("BATslice: boundary out of range\n");
		return NULL;
	}

	/*
	 * If the source BAT is readonly, then we can obtain a VIEW
	 * that just reuses the memory of the source.
	 */
	if (BAThrestricted(b) == BAT_READ && BATtrestricted(b) == BAT_READ) {
		BUN cnt = h - l;

		bn = VIEWcreate_(b, b, TRUE);
		bn->batFirst = bn->batDeleted = bn->batInserted = 0;
		bn->H->heap.base = (bn->htype) ? BUNhloc(bi, l) : NULL;
		bn->T->heap.base = (bn->ttype) ? BUNtloc(bi, l) : NULL;
		bn->H->heap.size = headsize(bn, cnt);
		bn->T->heap.size = tailsize(bn, cnt);
		BATsetcount(bn, cnt);
		BATsetcapacity(bn, cnt);
	/*
	 * We have to do it: create a new BAT and put everything into it.
	 */
	} else {
		BUN p = l;
		BUN q = h;

		bn = BATnew((BAThdense(b)?TYPE_void:b->htype), b->ttype, h - l, TRANSIENT);
		if (bn == NULL) {
			return bn;
		}
		if ((bn->htype == TYPE_void || !bn->hvarsized) &&
		    BATatoms[bn->htype].atomPut == NULL &&
		    BATatoms[bn->htype].atomFix == NULL &&
		    (bn->ttype == TYPE_void || !bn->tvarsized) &&
		    BATatoms[bn->ttype].atomPut == NULL &&
		    BATatoms[bn->ttype].atomFix == NULL) {
			if (bn->htype)
				memcpy(Hloc(bn, BUNfirst(bn)), Hloc(b, p),
				       (q - p) * Hsize(bn));
			if (bn->ttype)
				memcpy(Tloc(bn, BUNfirst(bn)), Tloc(b, p),
				       (q - p) * Tsize(bn));
			BATsetcount(bn, h - l);
		} else if (BAThdense(b) && b->ttype) {
			for (; p < q; p++) {
				bunfastapp(bn, BUNtail(bi, p));
			}
		} else if (b->htype != b->ttype || b->htype != TYPE_void) {
			for (; p < q; p++) {
				bunfastins(bn, BUNhead(bi, p), BUNtail(bi, p));
			}
		} else {
			BATsetcount(bn, h - l);
		}
		bn->hsorted = b->hsorted;
		bn->hrevsorted = b->hrevsorted;
		bn->hkey = b->hkey & 1;
		bn->H->nonil = b->H->nonil;
		bn->tsorted = b->tsorted;
		bn->trevsorted = b->trevsorted;
		bn->tkey = b->tkey & 1;
		bn->T->nonil = b->T->nonil;
	}
	bni = bat_iterator(bn);
	if (BAThdense(b)) {
		bn->hdense = TRUE;
		BATseqbase(bn, (oid) (b->hseqbase + low));
	} else if (bn->hkey && bn->htype == TYPE_oid) {
		if (BATcount(bn) == 0) {
			bn->hdense = TRUE;
			BATseqbase(bn, 0);
		} else if (bn->hsorted &&
			   (foid = *(oid *) BUNhloc(bni, BUNfirst(bn))) != oid_nil &&
			   foid + BATcount(bn) - 1 == *(oid *) BUNhloc(bni, BUNlast(bn) - 1)) {
			bn->hdense = TRUE;
			BATseqbase(bn, *(oid *) BUNhloc(bni, BUNfirst(bn)));
		}
	}
	if (BATtdense(b)) {
		bn->tdense = TRUE;
		BATseqbase(BATmirror(bn), (oid) (b->tseqbase + low));
	} else if (bn->tkey && bn->ttype == TYPE_oid) {
		if (BATcount(bn) == 0) {
			bn->tdense = TRUE;
			BATseqbase(BATmirror(bn), 0);
		} else if (bn->tsorted &&
			   (foid = *(oid *) BUNtloc(bni, BUNfirst(bn))) != oid_nil &&
			   foid + BATcount(bn) - 1 == *(oid *) BUNtloc(bni, BUNlast(bn) - 1)) {
			bn->tdense = TRUE;
			BATseqbase(BATmirror(bn), *(oid *) BUNtloc(bni, BUNfirst(bn)));
		}
	}
	bn->hsorted = BAThordered(b);
	bn->tsorted = BATtordered(b);
	bn->hrevsorted = BAThrevordered(b);
	bn->trevsorted = BATtrevordered(b);
	BATkey(bn, BAThkey(b));
	BATkey(BATmirror(bn), BATtkey(b));
	bn->H->nonil = b->H->nonil || bn->batCount == 0;
	bn->T->nonil = b->T->nonil || bn->batCount == 0;
	bn->H->nil = bn->T->nil = 0;	/* we just don't know */
	return bn;
      bunins_failed:
	BBPreclaim(bn);
	return NULL;
}

/*
 *  BAT Sorting
 * BATsort returns a sorted copy. BATorder sorts the BAT itself.
 */
int
BATordered(BAT *b)
{
	if (!b->hsorted)
		BATderiveHeadProps(b, 0);
	return b->hsorted;
}

int
BATordered_rev(BAT *b)
{
	if (!b->hrevsorted)
		BATderiveHeadProps(b, 0);
	return b->hrevsorted;
}

/* figure out which sort function is to be called
 * stable sort can produce an error (not enough memory available),
 * "quick" sort does not produce errors */
static gdk_return
do_sort(void *h, void *t, const void *base, size_t n, int hs, int ts, int tpe,
	int reverse, int stable)
{
	if (n <= 1)		/* trivially sorted */
		return GDK_SUCCEED;
	if (reverse) {
		if (stable) {
			if (GDKssort_rev(h, t, base, n, hs, ts, tpe) < 0) {
				return GDK_FAIL;
			}
		} else {
			GDKqsort_rev(h, t, base, n, hs, ts, tpe);
		}
	} else {
		if (stable) {
			if (GDKssort(h, t, base, n, hs, ts, tpe) < 0) {
				return GDK_FAIL;
			}
		} else {
			GDKqsort(h, t, base, n, hs, ts, tpe);
		}
	}
	return GDK_SUCCEED;
}

/* Sort b according to stable and reverse, do it in-place if copy is
 * unset, otherwise do it on a copy */
static BAT *
BATorder_internal(BAT *b, int stable, int reverse, int copy, const char *func)
{
	BATcheck(b, func);
	/* set some trivial properties (probably not necessary, but
	 * it's cheap) */
	if (b->htype == TYPE_void) {
		b->hsorted = 1;
		b->hrevsorted = b->hseqbase == oid_nil || b->batCount <= 1;
		b->hkey |= b->hseqbase != oid_nil;
	} else if (b->batCount <= 1) {
		b->hsorted = b->hrevsorted = 1;
	}
	if (reverse ? b->hrevsorted : b->hsorted) {
		/* b is already ordered as desired, hence we return b
		 * as is */
		return copy ? BATcopy(b, b->htype, b->ttype, FALSE, TRANSIENT) : b;
	}
	if (copy) {
		/* now make a writable copy that we're going to sort
		 * materialize any VOID columns while we're at it */
		b = BATcopy(b, BAThtype(b), BATttype(b), TRUE, TRANSIENT);
	} else if (b->ttype == TYPE_void && b->tseqbase != oid_nil) {
		/* materialize void-tail in-place */
		/* note, we don't need to materialize the head column:
		 * if it is void, either we didn't get here (already
		 * sorted correctly), or we will fall into BATrevert
		 * below which does the materialization for us */
		b = BATmaterializet(b);
		if (b == NULL)
			return NULL;
	}

	if ((reverse ? b->hsorted : b->hrevsorted) && (!stable || b->hkey)) {
		/* b is ordered in the opposite direction, hence we
		 * revert b (note that if requesting stable sort, the
		 * column needs to be key) */
		return BATrevert(b);
	}
	if (!(reverse ? b->hrevsorted : b->hsorted) &&
	    do_sort(Hloc(b, BUNfirst(b)), Tloc(b, BUNfirst(b)),
		    b->H->vheap ? b->H->vheap->base : NULL,
		    BATcount(b), Hsize(b), Tsize(b), b->htype,
		    reverse, stable) == GDK_FAIL) {
		if (copy)
			BBPreclaim(b);
		return NULL;
	}
	if (reverse) {
		b->hrevsorted = 1;
		b->hsorted = b->batCount <= 1;
	} else {
		b->hsorted = 1;
		b->hrevsorted = b->batCount <= 1;
	}
	b->tsorted = b->trevsorted = 0;
	HASHdestroy(b);
	IMPSdestroy(b);
	ALIGNdel(b, func, FALSE);
	b->hdense = 0;
	b->tdense = 0;
	b->batDirtydesc = b->H->heap.dirty = b->T->heap.dirty = TRUE;

	return b;
}

#undef BATorder
#undef BATorder_rev
#undef BATsort
#undef BATsort_rev
#undef BATssort
#undef BATssort_rev

BAT *
BATorder(BAT *b)
{
	return BATorder_internal(b, 0, 0, 0, "BATorder");
}

BAT *
BATorder_rev(BAT *b)
{
	return BATorder_internal(b, 0, 1, 0, "BATorder_rev");
}

BAT *
BATsort(BAT *b)
{
	return BATorder_internal(b, 0, 0, 1, "BATsort");
}

BAT *
BATsort_rev(BAT *b)
{
	return BATorder_internal(b, 0, 1, 1, "BATsort_rev");
}

BAT *
BATssort(BAT *b)
{
	return BATorder_internal(b, 1, 0, 1, "BATssort");
}

BAT *
BATssort_rev(BAT *b)
{
	return BATorder_internal(b, 1, 1, 1, "BATssort_rev");
}

/* subsort the bat b according to both o and g.  The stable and
 * reverse parameters indicate whether the sort should be stable or
 * descending respectively.  The parameter b is required, o and g are
 * optional (i.e., they may be NULL).
 *
 * A sorted copy is returned through the sorted parameter, the new
 * ordering is returned through the order parameter, group information
 * is returned through the groups parameter.  All three output
 * parameters may be NULL.  If they're all NULL, this function does
 * nothing.
 *
 * All BATs involved must be dense-headed.
 *
 * If o is specified, it is used to first rearrange b according to the
 * order specified in o, after which b is sorted taking g into
 * account.
 *
 * If g is specified, it indicates groups which should be individually
 * ordered.  Each row of consecutive equal values in g indicates a
 * group which is sorted according to stable and reverse.  g is used
 * after the order in b was rearranged according to o.
 *
 * The outputs order and groups can be used in subsequent calls to
 * this function.  This can be used if multiple BATs need to be sorted
 * together.  The BATs should then be sorted in order of significance,
 * and each following call should use the original unordered BAT plus
 * the order and groups bat from the previous call.  In this case, the
 * sorted BATs are not of much use, so the sorted output parameter
 * does not need to be specified.
 */
gdk_return
BATsubsort(BAT **sorted, BAT **order, BAT **groups,
	   BAT *b, BAT *o, BAT *g, int reverse, int stable)
{
	BAT *bn = NULL, *on = NULL, *gn = NULL;
	oid *grps, prev;
	BUN p, q, r;

	if (b == NULL || !BAThdense(b)) {
		GDKerror("BATsubsort: b must be dense-headed\n");
		return GDK_FAIL;
	}
	if (o != NULL &&
	    (!BAThdense(o) ||		       /* dense head */
	     ATOMtype(o->ttype) != TYPE_oid || /* oid tail */
	     BATcount(o) != BATcount(b) ||     /* same size as b */
	     (o->ttype == TYPE_void &&	       /* no nil tail */
	      BATcount(o) != 0 &&
	      o->tseqbase == oid_nil))) {
		GDKerror("BATsubsort: o must be [dense,oid] and same size as b\n");
		return GDK_FAIL;
	}
	if (g != NULL &&
	    (!BAThdense(g) ||		       /* dense head */
	     ATOMtype(g->ttype) != TYPE_oid || /* oid tail */
	     !g->tsorted ||		       /* sorted */
	     BATcount(o) != BATcount(b) ||     /* same size as b */
	     (g->ttype == TYPE_void &&	       /* no nil tail */
	      BATcount(g) != 0 &&
	      g->tseqbase == oid_nil))) {
		GDKerror("BATsubsort: g must be [dense,oid], sorted on the tail, and same size as b\n");
		return GDK_FAIL;
	}
	assert(reverse == 0 || reverse == 1);
	assert(stable == 0 || stable == 1);
	if (sorted == NULL && order == NULL && groups == NULL) {
		/* no place to put result, so we're done quickly */
		return GDK_SUCCEED;
	}
	if (BATcount(b) <= 1 ||
	    ((reverse ? BATtrevordered(b) : BATtordered(b)) &&
	     o == NULL && g == NULL &&
	     (groups == NULL || BATtkey(b) ||
	      (reverse ? BATtordered(b) : BATtrevordered(b))))) {
		/* trivially (sub)sorted, and either we don't need to
		 * return group information, or we can trivially
		 * deduce the groups */
		if (sorted) {
			BBPfix(b->batCacheid);
			bn = b;
			*sorted = bn;
		}
		if (order) {
			on = BATnew(TYPE_void, TYPE_void, BATcount(b), TRANSIENT);
			if (on == NULL)
				goto error;
			BATsetcount(on, BATcount(b));
			BATseqbase(on, 0);
			BATseqbase(BATmirror(on), 0);
			*order = on;
		}
		if (groups) {
			if (BATtkey(b)) {
				/* singleton groups */
				gn = BATnew(TYPE_void, TYPE_void, BATcount(b), TRANSIENT);
				if (gn == NULL)
					goto error;
				BATsetcount(gn, BATcount(b));
				BATseqbase(BATmirror(gn), 0);
			} else {
				/* single group */
				const oid *o = 0;
				assert(BATcount(b) == 1 ||
				       (BATtordered(b) && BATtrevordered(b)));
				gn = BATconstant(TYPE_oid, &o, BATcount(b), TRANSIENT);
				if (gn == NULL)
					goto error;
			}
			BATseqbase(gn, 0);
			*groups = gn;
		}
		return GDK_SUCCEED;
	}
	if (o) {
		bn = BATproject(o, b);
		if (bn == NULL)
			goto error;
		if (bn->ttype == TYPE_void || isVIEW(bn)) {
			b = BATcopy(bn, TYPE_void, ATOMtype(bn->ttype), TRUE, TRANSIENT);
			BBPunfix(bn->batCacheid);
			bn = b;
		}
	} else {
		bn = BATcopy(b, TYPE_void, b->ttype, TRUE, TRANSIENT);
	}
	if (bn == NULL)
		goto error;
	if (order) {
		/* prepare order bat */
		if (o) {
			/* make copy of input so that we can refine it
			 * copy can be read-only if we take the shortcut
			 * below in the case g is "key" */
			on = BATcopy(o, TYPE_void, TYPE_oid,
				     g == NULL ||
				     !(g->tkey || g->ttype == TYPE_void),
				     TRANSIENT);
			if (on == NULL)
				goto error;
		} else {
			/* create new order */
			on = BATnew(TYPE_void, TYPE_oid, BATcount(bn), TRANSIENT);
			if (on == NULL)
				goto error;
			grps = (oid *) Tloc(on, BUNfirst(on));
			for (p = 0, q = BATcount(bn); p < q; p++)
				grps[p] = p;
			BATsetcount(on, BATcount(bn));
			on->tkey = 1;
		}
		BATseqbase(on, 0);
		on->tsorted = on->trevsorted = 0;
		on->tdense = 0;
		*order = on;
	}
	if (g) {
		if (g->tkey || g->ttype == TYPE_void) {
			/* if g is "key", all groups are size 1, so no
			 * subsorting needed */
			if (sorted) {
				*sorted = bn;
			} else {
				BBPunfix(bn->batCacheid);
			}
			if (order)
				*order = on;
			if (groups) {
				BBPfix(g->batCacheid);
				*groups = g;
			}
			return GDK_SUCCEED;
		}
		assert(g->ttype == TYPE_oid);
		grps = (oid *) Tloc(g, BUNfirst(g));
		prev = grps[0];
		for (r = 0, p = 1, q = BATcount(g); p < q; p++) {
			if (grps[p] != prev) {
				/* sub sort [r,p) */
				if (do_sort(Tloc(bn, BUNfirst(bn) + r),
					    on ? Tloc(on, BUNfirst(on) + r) : NULL,
					    bn->T->vheap ? bn->T->vheap->base : NULL,
					    p - r, Tsize(bn), on ? Tsize(on) : 0,
					    bn->ttype, reverse, stable) == GDK_FAIL)
					goto error;
				r = p;
				prev = grps[p];
			}
		}
		/* sub sort [r,q) */
		if (do_sort(Tloc(bn, BUNfirst(bn) + r),
			    on ? Tloc(on, BUNfirst(on) + r) : NULL,
			    bn->T->vheap ? bn->T->vheap->base : NULL,
			    p - r, Tsize(bn), on ? Tsize(on) : 0,
			    bn->ttype, reverse, stable) == GDK_FAIL)
			goto error;
		/* if single group (r==0) the result is (rev)sorted,
		 * otherwise not */
		bn->tsorted = r == 0 && !reverse;
		bn->trevsorted = r == 0 && reverse;
	} else {
		if (b->ttype == TYPE_void) {
			b->tsorted = 1;
			b->trevsorted = b->tseqbase == oid_nil || b->batCount <= 1;
			b->tkey |= b->tseqbase != oid_nil;
		} else if (b->batCount <= 1) {
			b->tsorted = b->trevsorted = 1;
		}
		if (!(reverse ? bn->trevsorted : bn->tsorted) &&
		    do_sort(Tloc(bn, BUNfirst(bn)),
			    on ? Tloc(on, BUNfirst(on)) : NULL,
			    bn->T->vheap ? bn->T->vheap->base : NULL,
			    BATcount(bn), Tsize(bn), on ? Tsize(on) : 0,
			    bn->ttype, reverse, stable) == GDK_FAIL)
			goto error;
		bn->tsorted = !reverse;
		bn->trevsorted = reverse;
	}
	if (groups) {
		if (BATgroup_internal(groups, NULL, NULL, bn, g, NULL, NULL, 1) == GDK_FAIL)
			goto error;
		if ((*groups)->tkey && (bn->tsorted || bn->trevsorted)) {
			/* if new groups bat is key and the result bat
			 * is (rev)sorted (single input group), we
			 * know it is key */
			bn->tkey = 1;
		}
	}

	if (sorted)
		*sorted = bn;
	else
		BBPunfix(bn->batCacheid);

	return GDK_SUCCEED;

  error:
	if (bn)
		BBPunfix(bn->batCacheid);
	if (on)
		BBPreclaim(on);
	if (gn)
		BBPreclaim(gn);
	if (sorted)
		*sorted = NULL;
	if (order)
		*order = NULL;
	if (groups)
		*groups = NULL;
	return GDK_FAIL;
}

/*
 * Reverse a BAT
 * BATrevert rearranges a BAT in reverse order.
 */
BAT *
BATrevert(BAT *b)
{
	char *h, *t;
	BUN p, q;
	BATiter bi = bat_iterator(b);
	size_t s;
	int x;

	BATcheck(b, "BATrevert");
	if ((b->htype == TYPE_void && b->hseqbase != oid_nil) || (b->ttype == TYPE_void && b->tseqbase != oid_nil)) {
		/* materialize void columns in-place */
		b = BATmaterialize(b);
		if (b == NULL)
			return NULL;
	}
	ALIGNdel(b, "BATrevert", FALSE);
	s = Hsize(b);
	if (s > 0) {
		h = (char *) GDKmalloc(s);
		if (!h) {
			return NULL;
		}
		for (p = BUNlast(b) ? BUNlast(b) - 1 : 0, q = BUNfirst(b); p > q; p--, q++) {
			char *ph = BUNhloc(bi, p);
			char *qh = BUNhloc(bi, q);

			memcpy(h, ph, s);
			memcpy(ph, qh, s);
			memcpy(qh, h, s);
		}
		GDKfree(h);
	}
	s = Tsize(b);
	if (s > 0) {
		t = (char *) GDKmalloc(s);
		if (!t) {
			return NULL;
		}
		for (p = BUNlast(b) ? BUNlast(b) - 1 : 0, q = BUNfirst(b); p > q; p--, q++) {
			char *pt = BUNtloc(bi, p);
			char *qt = BUNtloc(bi, q);

			memcpy(t, pt, s);
			memcpy(pt, qt, s);
			memcpy(qt, t, s);
		}
		GDKfree(t);
	}
	HASHdestroy(b);
	IMPSdestroy(b);
	/* interchange sorted and revsorted */
	x = b->hrevsorted;
	b->hrevsorted = b->hsorted;
	b->hsorted = x;
	x = b->trevsorted;
	b->trevsorted = b->tsorted;
	b->tsorted = x;
	b->hdense = FALSE;
	b->tdense = FALSE;
	return b;
}

/*
 * return a view on b consisting of the head of b and a new dense tail
 * starting at oid_base.
 */
BAT *
BATmark(BAT *b, oid oid_base)
{
	BAT *bn;

	BATcheck(b, "BATmark");
	bn = VIEWhead(b);
	if (bn) {
		BATseqbase(BATmirror(bn), oid_base);
		if (oid_base == oid_nil)
			bn->T->nonil = 0;
		if (BAThrestricted(b) != BAT_READ) {
			BAT *v = bn;

			bn = BATcopy(v, v->htype, v->ttype, TRUE, TRANSIENT);
			BBPreclaim(v);
		}
	}
	return bn;
}

#define mark_grp_init(BUNfnd)				\
	do {						\
		BUN w;					\
							\
		BUNfnd(w, gi, &v);			\
		if (w != BUN_NONE) {			\
			n = * (oid *) BUNtloc(gi, w);	\
		} else {				\
			n = oid_nil;			\
		}					\
	} while (0)

#define mark_grp_loop4(BUNhead, BUNtail, init_n)	\
	do {							\
		oid u = oid_nil;				\
		oid n = oid_nil;				\
								\
		bn->T->nil = 0;					\
		BATloop(b, p, q) {				\
			oid v = * (oid *) BUNtail(bi, p);	\
								\
			if (v != u) {				\
				init_n;				\
				u = v;				\
			} else if (n != oid_nil) {		\
				n++;				\
			}					\
			if (n == oid_nil)			\
				bn->T->nil =1;			\
			bunfastins_nocheck_inc(bn, r, BUNhead(bi, p), &n); \
		}						\
		bn->T->nonil = !bn->T->nil;			\
	} while (0)

#define mark_grp_loop3(BUNhead, BUNtail, BUNfnd)			\
	do {								\
		bn->T->nil = 0;						\
		BATloop(b, p, q) {					\
			oid n = oid_nil;				\
			BUN w;						\
			const void *v = BUNtail(bi, p);			\
			oid *m;						\
									\
			BUNfnd(w, gci, v);				\
			if (w != BUN_NONE && *(m = (oid*) BUNtloc(gci, w)) != oid_nil) { \
				n = (*m)++;				\
			} else						\
				bn->T->nil = 1;				\
			bunfastins_nocheck_inc(bn, r, BUNhead(bi, p), &n); \
		}							\
		bn->T->nonil = !bn->T->nil;				\
	} while (0)

#define mark_grp_loop2(BUNhead, BUNtail)				\
	do {								\
		if (gc) {						\
			BATiter gci = bat_iterator(gc);			\
									\
			if (BAThdense(gc)) {				\
				mark_grp_loop3(BUNhead,			\
					       BUNtail, BUNfndVOID);	\
			} else {					\
				mark_grp_loop3(BUNhead,			\
					       BUNtail, BUNfndOID);	\
			}						\
		} else {						\
			if (s) {					\
				mark_grp_loop4(BUNhead,			\
					       BUNtail, n = *s);	\
			} else {					\
				if (BAThdense(g)) {			\
					mark_grp_loop4(BUNhead,		\
						       BUNtail,		\
						       mark_grp_init(BUNfndVOID)); \
				} else {				\
					mark_grp_loop4(BUNhead,		\
						       BUNtail,		\
						       mark_grp_init(BUNfndOID)); \
				}					\
			}						\
		}							\
	} while (0)

BAT *
BATmark_grp(BAT *b, BAT *g, oid *s)
{
	BAT *bn = NULL, *gc = NULL;
	bit trivprop = FALSE;

	BATcheck(b, "BATmark_grp");
	BATcheck(g, "BATmark_grp");
	ERRORcheck(b->ttype != TYPE_void && b->ttype != TYPE_oid,
		   "BATmark_grp: tail of BAT b must be oid.\n");
	ERRORcheck(g->htype != TYPE_void && g->htype != TYPE_oid,
		   "BATmark_grp: head of BAT g must be oid.\n");
	ERRORcheck(b->ttype == TYPE_void && b->tseqbase == oid_nil,
		   "BATmark_grp: tail of BAT b must not be nil.\n");
	ERRORcheck(g->htype == TYPE_void && g->hseqbase == oid_nil,
		   "BATmark_grp: head of BAT g must not be nil.\n");
	ERRORcheck(s && *s == oid_nil,
		   "BATmark_grp: base oid s must not be nil.\n");
	ERRORcheck(!s && g->ttype != TYPE_oid,
		   "BATmark_grp: tail of BAT g must be oid.\n");

	if (BATcount(b) == 0 || BATcount(g) == 1) {
		if (s) {
			return BATmark(b, *s);
		} else {
			BATiter gi = bat_iterator(g);

			return BATmark(b, *(oid *) BUNtloc(gi, BUNfirst(g)));
		}
	}

	if (!BATtordered(b)) {
		if (s) {
			BUN p, q, r;

			if (BAThdense(g)) {
				gc = BATnew(TYPE_void, TYPE_oid, BATcount(g), TRANSIENT);
				if (gc == NULL)
					return NULL;
				r = BUNfirst(gc);
				BATloop(g, p, q) {
					bunfastins_nocheck_inc(gc, r, NULL, s);
				}
			} else {
				BATiter gi = bat_iterator(g);

				gc = BATnew(TYPE_oid, TYPE_oid, BATcount(g), TRANSIENT);
				if (gc == NULL)
					return NULL;
				r = BUNfirst(gc);
				BATloop(g, p, q) {
					bunfastins_nocheck_inc(gc, r, BUNhloc(gi, p), s);
				}
			}
			BATsetcount(gc, BATcount(g));
			gc->hdense = BAThdense(g);
			if (gc->hdense) {
				BATseqbase(gc, g->hseqbase);
				gc->hsorted = 1;
			} else {
				gc->hsorted = BAThordered(g);
			}
			gc->hrevsorted = BAThrevordered(g);
			BATkey(gc, (gc->hdense || g->hkey != FALSE));
			gc->H->nonil = g->H->nonil;
			gc->T->nonil = 1;
		} else {
			gc = BATcopy(g, g->htype, g->ttype, TRUE, TRANSIENT);
			if (gc == NULL)
				return NULL;
		}
	}
	bn = BATnew(b->htype, TYPE_oid, BATcount(b), TRANSIENT);
	if (bn == NULL) {
		if (gc)
			BBPreclaim(gc);
		return NULL;
	}

	{
		BUN p, q, r = BUNfirst(bn);
		BATiter bi = bat_iterator(b);
		BATiter gi = bat_iterator(g);

		if (b->hvarsized) {
			if (b->ttype == TYPE_void) {
				mark_grp_loop2(BUNhvar, BUNtvar);
			} else {
				mark_grp_loop2(BUNhvar, BUNtloc);
			}
		} else {
			if (b->ttype == TYPE_void) {
				mark_grp_loop2(BUNhloc, BUNtvar);
			} else {
				mark_grp_loop2(BUNhloc, BUNtloc);
			}
		}
		BATsetcount(bn, BATcount(b));
	}

	trivprop = (BATcount(bn) < 2);
	ALIGNsetH(bn, b);
	bn->hsorted = trivprop || BAThordered(b);
	bn->hrevsorted = trivprop || BAThrevordered(b);
	bn->hdense = BAThdense(b) || (trivprop && b->htype == TYPE_oid);
	if (bn->hdense) {
		BATseqbase(bn, b->hseqbase);
	}
	BATkey(bn, (bn->hdense || b->hkey != FALSE));
	bn->tsorted = trivprop;
	bn->trevsorted = trivprop;
	bn->tdense = trivprop;
	if (BATtdense(bn)) {
		BATiter bni = bat_iterator(bn);

		BATseqbase(BATmirror(bn), *(oid *) BUNtloc(bni, BUNfirst(bn)));
	}
	BATkey(BATmirror(bn), trivprop);
	bn->H->nonil = b->H->nonil;

	if (gc)
		BBPreclaim(gc);
	return bn;
  bunins_failed:
  hashfnd_failed:
	if (gc)
		BBPreclaim(gc);
	if (bn)
		BBPreclaim(bn);
	return NULL;
}

/* return a new BAT of length n with a dense head and the constant v
 * in the tail */
BAT *
BATconstant(int tailtype, const void *v, BUN n, int role)
{
	BAT *bn;
	void *p;
	BUN i;

	if (v == NULL)
		return NULL;
	bn = BATnew(TYPE_void, tailtype, n, role);
	if (bn == NULL)
		return NULL;
	p = Tloc(bn, bn->batFirst);
	switch (ATOMstorage(tailtype)) {
	case TYPE_void:
		v = &oid_nil;
		BATseqbase(BATmirror(bn), oid_nil);
		bn->T->nil = n >= 1;
		break;
	case TYPE_bte:
		for (i = 0; i < n; i++)
			((bte *) p)[i] = *(bte *) v;
		bn->T->nil = n >= 1 && *(bte *) v == bte_nil;
		break;
	case TYPE_sht:
		for (i = 0; i < n; i++)
			((sht *) p)[i] = *(sht *) v;
		bn->T->nil = n >= 1 && *(sht *) v == sht_nil;
		break;
	case TYPE_int:
	case TYPE_flt:
		assert(sizeof(int) == sizeof(flt));
		for (i = 0; i < n; i++)
			((int *) p)[i] = *(int *) v;
		bn->T->nil = n >= 1 && (ATOMstorage(tailtype) == TYPE_int ? *(int *) v == int_nil : *(flt *) v == flt_nil);
		break;
	case TYPE_lng:
	case TYPE_dbl:
		assert(sizeof(lng) == sizeof(dbl));
		for (i = 0; i < n; i++)
			((lng *) p)[i] = *(lng *) v;
		bn->T->nil = n >= 1 && (ATOMstorage(tailtype) == TYPE_lng ? *(lng *) v == lng_nil : *(dbl *) v == dbl_nil);
		break;
	default:
		bn->T->nil = n >= 1 && ATOMcmp(tailtype, v, ATOMnilptr(tailtype)) == 0;
		for (i = BUNfirst(bn), n += i; i < n; i++)
			tfastins_nocheck(bn, i, v, Tsize(bn));
		n -= BUNfirst(bn);
		break;
	}
	BATsetcount(bn, n);
	bn->tsorted = 1;
	bn->trevsorted = 1;
	bn->T->nonil = !bn->T->nil;
	bn->T->key = BATcount(bn) <= 1;
	BATseqbase(bn, 0);
	return bn;

  bunins_failed:
	BBPreclaim(bn);
	return NULL;
}

/* return a new bat which is aligned with b and with the constant v in
 * the tail */
BAT *
BATconst(BAT *b, int tailtype, const void *v, int role)
{
	BAT *bn;

	BATcheck(b, "BATconst");
	bn = BATconstant(tailtype, v, BATcount(b), role);
	if (bn == NULL)
		return NULL;
	if (b->H->type != bn->H->type) {
		BAT *bnn = VIEWcreate(b, bn);

		BBPunfix(bn->batCacheid);
		bn = bnn;
	} else {
		BATseqbase(bn, b->hseqbase);
	}

	return bn;
}

/*
 * BAT Aggregates
 *
 * We retain the size() and card() aggregate results in the column
 * descriptor.  We would like to have such functionality in an
 * extensible way for many aggregates, for DD (1) we do not want to
 * change the binary BAT format on disk and (2) aggr and size are the
 * most relevant aggregates.
 *
 * It is all hacked into the aggr[3] records; three adjacent integers
 * that were left over in the column record. We refer to these as if
 * it where an int aggr[3] array.  The below routines set and retrieve
 * the aggregate values from the tail of the BAT, as many
 * aggregate-manipulating BAT functions work on tail.
 *
 * The rules are as follows: aggr[0] contains the alignment ID of the
 * column (if set i.e. nonzero).  Hence, if this value is nonzero and
 * equal to b->talign, the precomputed aggregate values in
 * aggr[GDK_AGGR_SIZE] and aggr[GDK_AGGR_CARD] hold. However, only one
 * of them may be set at the time. This is encoded by the value
 * int_nil, which cannot occur in these two aggregates.
 *
 * This was now extended to record the property whether we know there
 * is a nil value present by mis-using the highest bits of both
 * GDK_AGGR_SIZE and GDK_AGGR_CARD.
 */

void
PROPdestroy(PROPrec *p)
{
	PROPrec *n;

	while (p) {
		n = p->next;
		if (p->v.vtype == TYPE_str)
			GDKfree(p->v.val.sval);
		GDKfree(p);
		p = n;
	}
}

PROPrec *
BATgetprop(BAT *b, int idx)
{
	PROPrec *p = b->T->props;

	while (p) {
		if (p->id == idx)
			return p;
		p = p->next;
	}
	return NULL;
}

void
BATsetprop(BAT *b, int idx, int type, void *v)
{
	ValRecord vr;
	PROPrec *p = BATgetprop(b, idx);

	if (!p && (p = (PROPrec *) GDKmalloc(sizeof(PROPrec))) != NULL) {
		p->id = idx;
		p->next = b->T->props;
		p->v.vtype = 0;
		b->T->props = p;
	}
	if (p) {
		VALset(&vr, type, v);
		VALcopy(&p->v, &vr);
		b->batDirtydesc = TRUE;
	}
}


/*
 * The BATcount_no_nil function counts all BUN in a BAT that have a
 * non-nil tail value.
 */
BUN
BATcount_no_nil(BAT *b)
{
	BUN cnt = 0;
	BUN i, n;
	const void *p, *nil;
	const char *base;
	int t;
	int (*cmp)(const void *, const void *);

	BATcheck(b, "BATcnt");
	n = BATcount(b);
	if (b->T->nonil)
		return n;
	p = Tloc(b, b->batFirst);
	t = b->ttype;
	if (t != ATOMstorage(t) &&
	    ATOMnilptr(ATOMstorage(t)) == ATOMnilptr(t) &&
	    BATatoms[ATOMstorage(t)].atomCmp == BATatoms[t].atomCmp)
		t = ATOMstorage(t);
	switch (t) {
	case TYPE_void:
		cnt = b->tseqbase == oid_nil ? 0 : n;
		break;
	case TYPE_bte:
		for (i = 0; i < n; i++)
			cnt += ((const bte *) p)[i] != bte_nil;
		break;
	case TYPE_sht:
		for (i = 0; i < n; i++)
			cnt += ((const sht *) p)[i] != sht_nil;
		break;
	case TYPE_int:
		for (i = 0; i < n; i++)
			cnt += ((const int *) p)[i] != int_nil;
		break;
	case TYPE_lng:
		for (i = 0; i < n; i++)
			cnt += ((const lng *) p)[i] != lng_nil;
		break;
	case TYPE_flt:
		for (i = 0; i < n; i++)
			cnt += ((const flt *) p)[i] != flt_nil;
		break;
	case TYPE_dbl:
		for (i = 0; i < n; i++)
			cnt += ((const dbl *) p)[i] != dbl_nil;
		break;
	case TYPE_str:
		base = b->T->vheap->base;
		switch (b->T->width) {
		case 1:
			for (i = 0; i < n; i++)
				cnt += base[((var_t) ((const unsigned char *) p)[i] + GDK_VAROFFSET) << GDK_VARSHIFT] != '\200';
			break;
		case 2:
			for (i = 0; i < n; i++)
				cnt += base[((var_t) ((const unsigned short *) p)[i] + GDK_VAROFFSET) << GDK_VARSHIFT] != '\200';
			break;
#if SIZEOF_VAR_T != SIZEOF_INT
		case 4:
			for (i = 0; i < n; i++)
				cnt += base[(var_t) ((const unsigned int *) p)[i] << GDK_VARSHIFT] != '\200';
			break;
#endif
		default:
			for (i = 0; i < n; i++)
				cnt += base[((const var_t *) p)[i] << GDK_VARSHIFT] != '\200';
			break;
		}
		break;
	default:
		nil = ATOMnilptr(t);
		cmp = BATatoms[t].atomCmp;
		if (b->tvarsized) {
			base = b->T->vheap->base;
			for (i = 0; i < n; i++)
				cnt += (*cmp)(nil, base + ((const var_t *) p)[i]) != 0;
		} else {
			for (i = BUNfirst(b), n += i; i < n; i++)
				cnt += (*cmp)(Tloc(b, i), nil) != 0;
		}
		break;
	}
	if (cnt == BATcount(b)) {
		/* we learned something */
		b->T->nonil = 1;
		assert(b->T->nil == 0);
		b->T->nil = 0;
	}
	return cnt;
}

static BAT *
newdensecand(oid first, oid last)
{
	BAT *bn;

	if ((bn = BATnew(TYPE_void, TYPE_void, 0, TRANSIENT)) == NULL)
		return NULL;
	if (last < first)
		first = last = 0; /* empty range */
	BATsetcount(bn, last - first + 1);
	BATseqbase(bn, 0);
	BATseqbase(BATmirror(bn), first);
	return bn;
}

/* merge two candidate lists and produce a new one
 *
 * candidate lists are VOID-headed BATs with an OID tail which is
 * sorted and unique.
 */
BAT *
BATmergecand(BAT *a, BAT *b)
{
	BAT *bn;
	const oid *ap, *bp, *ape, *bpe;
	oid *p, i;
	oid af, al, bf, bl;
	BATiter ai, bi;
	bit ad, bd;

	BATcheck(a, "BATmergecand");
	BATcheck(b, "BATmergecand");
	assert(a->htype == TYPE_void);
	assert(b->htype == TYPE_void);
	assert(ATOMtype(a->ttype) == TYPE_oid);
	assert(ATOMtype(b->ttype) == TYPE_oid);
	assert(BATcount(a) <= 1 || a->tsorted);
	assert(BATcount(b) <= 1 || b->tsorted);
	assert(BATcount(a) <= 1 || a->tkey);
	assert(BATcount(b) <= 1 || b->tkey);
	assert(a->T->nonil);
	assert(b->T->nonil);

	/* we can return a if b is empty (and v.v.) */
	if (BATcount(a) == 0) {
		return BATcopy(b, b->htype, b->ttype, 0, TRANSIENT);
	}
	if (BATcount(b) == 0) {
		return BATcopy(a, a->htype, a->ttype, 0, TRANSIENT);
	}
	/* we can return a if a fully covers b (and v.v) */
	ai = bat_iterator(a);
	bi = bat_iterator(b);
	af = *(oid*) BUNtail(ai, BUNfirst(a));
	bf = *(oid*) BUNtail(bi, BUNfirst(b));
	al = *(oid*) BUNtail(ai, BUNlast(a) - 1);
	bl = *(oid*) BUNtail(bi, BUNlast(b) - 1);
	ad = (af + BATcount(a) - 1 == al); /* i.e., dense */
	bd = (bf + BATcount(b) - 1 == bl); /* i.e., dense */
	if (ad && bd) {
		/* both are dense */
		if (af <= bf && bf <= al + 1) {
			/* partial overlap starting with a, or b is
			 * smack bang after a */
			return newdensecand(af, al < bl ? bl : al);
		}
		if (bf <= af && af <= bl + 1) {
			/* partial overlap starting with b, or a is
			 * smack bang after b */
			return newdensecand(bf, al < bl ? bl : al);
		}
	}
	if (ad && af <= bf && al >= bl) {
		return newdensecand(af, al);
	}
	if (bd && bf <= af && bl >= al) {
		return newdensecand(bf, bl);
	}

	bn = BATnew(TYPE_void, TYPE_oid, BATcount(a) + BATcount(b), TRANSIENT);
	if (bn == NULL)
		return NULL;
	p = (oid *) Tloc(bn, BUNfirst(bn));
	if (a->ttype == TYPE_void && b->ttype == TYPE_void) {
		/* both lists are VOID */
		if (a->tseqbase > b->tseqbase) {
			BAT *t = a;

			a = b;
			b = t;
		}
		/* a->tseqbase <= b->tseqbase */
		for (i = a->tseqbase; i < a->tseqbase + BATcount(a); i++)
			*p++ = i;
		for (i = MAX(b->tseqbase, i);
		     i < b->tseqbase + BATcount(b);
		     i++)
			*p++ = i;
	} else if (a->ttype == TYPE_void || b->ttype == TYPE_void) {
		if (b->ttype == TYPE_void) {
			BAT *t = a;

			a = b;
			b = t;
		}
		/* a->ttype == TYPE_void, b->ttype == TYPE_oid */
		bp = (const oid *) Tloc(b, BUNfirst(b));
		bpe = bp + BATcount(b);
		while (bp < bpe && *bp < a->tseqbase)
			*p++ = *bp++;
		for (i = a->tseqbase; i < a->tseqbase + BATcount(a); i++)
			*p++ = i;
		while (bp < bpe && *bp < i)
			bp++;
		while (bp < bpe)
			*p++ = *bp++;
	} else {
		/* a->ttype == TYPE_oid, b->ttype == TYPE_oid */
		ap = (const oid *) Tloc(a, BUNfirst(a));
		ape = ap + BATcount(a);
		bp = (const oid *) Tloc(b, BUNfirst(b));
		bpe = bp + BATcount(b);
		while (ap < ape && bp < bpe) {
			if (*ap < *bp)
				*p++ = *ap++;
			else if (*ap > *bp)
				*p++ = *bp++;
			else {
				*p++ = *ap++;
				bp++;
			}
		}
		while (ap < ape)
			*p++ = *ap++;
		while (bp < bpe)
			*p++ = *bp++;
	}

	/* properties */
	BATsetcount(bn, (BUN) (p - (oid *) Tloc(bn, BUNfirst(bn))));
	BATseqbase(bn, 0);
	bn->trevsorted = BATcount(bn) <= 1;
	bn->tsorted = 1;
	bn->tkey = 1;
	bn->T->nil = 0;
	bn->T->nonil = 1;
	return virtualize(bn);
}

/* intersect two candidate lists and produce a new one
 *
 * candidate lists are VOID-headed BATs with an OID tail which is
 * sorted and unique.
 */
BAT *
BATintersectcand(BAT *a, BAT *b)
{
	BAT *bn;
	const oid *ap, *bp, *ape, *bpe;
	oid *p;
	oid af, al, bf, bl;
	BATiter ai, bi;

	BATcheck(a, "BATintersectcand");
	BATcheck(b, "BATintersectcand");
	assert(a->htype == TYPE_void);
	assert(b->htype == TYPE_void);
	assert(ATOMtype(a->ttype) == TYPE_oid);
	assert(ATOMtype(b->ttype) == TYPE_oid);
	assert(a->tsorted);
	assert(b->tsorted);
	assert(a->tkey);
	assert(b->tkey);
	assert(a->T->nonil);
	assert(b->T->nonil);

	if (BATcount(a) == 0 || BATcount(b) == 0) {
		return newdensecand(0, 0);
	}

	ai = bat_iterator(a);
	bi = bat_iterator(b);
	af = *(oid*) BUNtail(ai, BUNfirst(a));
	bf = *(oid*) BUNtail(bi, BUNfirst(b));
	al = *(oid*) BUNtail(ai, BUNlast(a) - 1);
	bl = *(oid*) BUNtail(bi, BUNlast(b) - 1);

	if ((af + BATcount(a) - 1 == al) && (bf + BATcount(b) - 1 == bl)) {
		/* both lists are VOID */
		return newdensecand(MAX(af, bf), MIN(al, bl));
	}

	bn = BATnew(TYPE_void, TYPE_oid, MIN(BATcount(a), BATcount(b)), TRANSIENT);
	if (bn == NULL)
		return NULL;
	p = (oid *) Tloc(bn, BUNfirst(bn));
	if (a->ttype == TYPE_void || b->ttype == TYPE_void) {
		if (b->ttype == TYPE_void) {
			BAT *t = a;

			a = b;
			b = t;
		}
		/* a->ttype == TYPE_void, b->ttype == TYPE_oid */
		bp = (const oid *) Tloc(b, BUNfirst(b));
		bpe = bp + BATcount(b);
		while (bp < bpe && *bp < a->tseqbase)
			bp++;
		while (bp < bpe && *bp < a->tseqbase + BATcount(a))
			*p++ = *bp++;
	} else {
		/* a->ttype == TYPE_oid, b->ttype == TYPE_oid */
		ap = (const oid *) Tloc(a, BUNfirst(a));
		ape = ap + BATcount(a);
		bp = (const oid *) Tloc(b, BUNfirst(b));
		bpe = bp + BATcount(b);
		while (ap < ape && bp < bpe) {
			if (*ap < *bp)
				ap++;
			else if (*ap > *bp)
				bp++;
			else {
				*p++ = *ap++;
				bp++;
			}
		}
	}

	/* properties */
	BATsetcount(bn, (BUN) (p - (oid *) Tloc(bn, BUNfirst(bn))));
	BATseqbase(bn, 0);
	bn->trevsorted = BATcount(bn) <= 1;
	bn->tsorted = 1;
	bn->tkey = 1;
	bn->T->nil = 0;
	bn->T->nonil = 1;
	return virtualize(bn);
}
