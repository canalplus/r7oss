#include "rlk_inic.h"

unsigned char *
IWREQdecode(struct iwreq *wrq, unsigned char *buf, IW_TYPE type, char *kernel_data)
{
	unsigned char *p = buf;
	unsigned short size = 0;
	u32 *pValue;

	switch (type)
	{
	case IW_NAME_TYPE:
		/*
		if (strlen(p) >= sizeof(wrq->u.name)) 
		{
			printk("ERROR(IW_NAME_TYPE): name too long: %d > %d\n",
					(u32)strlen(p), (u32)sizeof(wrq->u.name));
			return NULL;
		}
		*/
		memcpy(wrq->u.name, p, IFNAMSIZ);
		p += IFNAMSIZ;
		break;
	case IW_POINT_TYPE:
		wrq->u.data.length = *(u16 *)p; 
		p += 2;
		size = wrq->u.data.length;
		// padding 2 octets for 4 alignment
		p += 2;
		// kernel_data=NULL : data has been copied by copy_to_user
		if (kernel_data)
		{
			/* In new ioctl API, data may NOT been copied by copy_to_user. */
			/* (descr->token_size == 1) is always the case */
			memmove(kernel_data, p , size);
			p += size;
			p = (unsigned char *)ROUND_UP(p, 2);
		}
		wrq->u.data.flags = *(u16 *)p;
		p += 2;
		break;
	case IW_PARAM_TYPE:
		wrq->u.param.value = *(u32 *)p;
		p += 4;
		wrq->u.param.fixed = *p;
		p++;
		wrq->u.param.disabled = *p;
		p++;
		wrq->u.param.flags = *(u16 *)p; 
		p += 2;
		break;
	case IW_FREQ_TYPE:
		wrq->u.freq.m = *(s32 *)p;
		p += 4;
		wrq->u.freq.e = *(s16 *)p;
		p += 2;
		wrq->u.freq.i = *p;
		p++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
		wrq->u.freq.flags = *p; 
#else
		wrq->u.freq.pad   = *p; 
#endif
		p += 1;
		break;
	case IW_MODE_TYPE:
		wrq->u.mode = *(u32 *)p;
		p += 4;
		break;
	case IW_U32_TYPE:
		pValue = (u32 *) wrq;
		*(pValue) = *(u32 *)p;
		p += 4;
		*(pValue+1) = *(u32 *)p;
		p += 4;
		break;
	case IW_QUALITY_TYPE:
		wrq->u.qual.qual = *p;
		p++;
		wrq->u.qual.level = *p;
		p++;
		wrq->u.qual.noise = *p;
		p++;
		wrq->u.qual.updated = *p;
		p++;
		break;
	case IW_SOCKADDR_TYPE:
		wrq->u.addr.sa_family = *(unsigned short *)p; 
		p += 2;
		memmove(wrq->u.addr.sa_data, p, 14);
		p += 14;
		break;
	default:
		break;
	}
	return p;
}

unsigned char *
IWREQencode(unsigned char *buf, struct iwreq *wrq, IW_TYPE type, char *kernel_data)
{
	unsigned char *p = buf;
	unsigned short size = 0;
	u32 *pValue;

	switch (type)
	{
	case IW_NAME_TYPE:
		memcpy(p, wrq->u.name, IFNAMSIZ);
		p += IFNAMSIZ;
		break;
	case IW_POINT_TYPE:
		size = wrq->u.data.length;
		*(u16 *)p = size;
		p += 2;
		// padding 2 octets for 4 alignment
		p += 2;
		if (kernel_data && (size > 0))
		{
			memmove(p, kernel_data, size);
		}
		else if (!kernel_data)
		{
			int err = copy_from_user(p, wrq->u.data.pointer, size);
			if (err)
			{
				printk("ERROR: copy_from_user() fail!\n");
			}
		}
		p += size;
		p = (unsigned char *)ROUND_UP(p, 2);
		*(u16 *)p = wrq->u.data.flags; 
		p += 2;
		break;
	case IW_PARAM_TYPE:
		*(s32 *)p = wrq->u.param.value;
		p += 4;
		*p = wrq->u.param.fixed;
		p++;
		*p = wrq->u.param.disabled;
		p++;
		*(u16 *)p = wrq->u.param.flags; 
		p += 2;
		break;
	case IW_FREQ_TYPE:
		*(s32 *)p = wrq->u.freq.m;
		p += 4;
		*(s16 *)p = wrq->u.freq.e;
		p += 2;
		*p = wrq->u.freq.i;
		p++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
		*p = wrq->u.freq.flags; 
#else
		*p = wrq->u.freq.pad; 
		p += 1;
#endif
		break;
	case IW_MODE_TYPE:
		*(u32 *)p = wrq->u.mode;
		p += 4;
		break;
	case IW_U32_TYPE:
		pValue = (u32 *) wrq;
		*(u32 *)p = *(pValue);
		p += 4;
		*(u32 *)p = *(pValue+1);
		p += 4;
		break;
	case IW_QUALITY_TYPE:
		*p = wrq->u.qual.qual;
		p++;
		*p = wrq->u.qual.level;
		p++;
		*p = wrq->u.qual.noise;
		p++;
		*p = wrq->u.qual.updated;
		p++;
		break;
	case IW_SOCKADDR_TYPE:
		*(u16 *)p = wrq->u.addr.sa_family; 
		p += 2;
		memmove(p, wrq->u.addr.sa_data, 14);
		p += 14;
		break;
	default:
		break;
	}
	return p;
}


