#ifndef _ASM_ARM_ARCH_RESERVED_MEM_H
#define _ASM_ARM_ARCH_RESERVED_MEM_H


/*
 * Defualt reserved memory size
 * MFC		: 6 MB
 * Post		: 8 MB
 * JPEG		: 8 MB
 * CMM		: 8 MB
 * TV		: 8 MB
 * Camera	: 15 MB
 * These sizes can be modified
 */

// added by sy82.yoon(2009.04.24)
#define	DRAM_END_ADDR		(0x58000000)

#define	RESERVED_MEM_TVOUT		(1*1024*1024)
#define	RESERVED_MEM_CMM			(8*1024*1024)
#define	RESERVED_MEM_JPEG			(8*1024*1024)
#define	RESERVED_MEM_MFC			(6*1024*1024)
#define	RESERVED_MEM_POST			(1*1024*1024)
#define	RESERVED_MEM_CAMERA		(10*1024*1024)	
#define	RESERVED_MEM_GPU			(2*1024*1024)
#define	RESERVED_MEM_PMEM			(16*1024*1024)
#define	DRAM_OFFSET						(0x8000000)	//256MB: 0x8000000, 128MB: 0

//#define CONFIG_RESERVED_MEM_JPEG
//#define CONFIG_RESERVED_MEM_JPEG_POST
//#define CONFIG_RESERVED_MEM_MFC
//#define CONFIG_RESERVED_MEM_MFC_POST
//#define CONFIG_RESERVED_MEM_JPEG_MFC_POST
//#define CONFIG_RESERVED_MEM_JPEG_CAMERA
//#define CONFIG_RESERVED_MEM_JPEG_POST_CAMERA
//#define CONFIG_RESERVED_MEM_MFC_CAMERA
//#define CONFIG_RESERVED_MEM_MFC_POST_CAMERA
//#define CONFIG_RESERVED_MEM_JPEG_MFC_POST_CAMERA
//#define CONFIG_RESERVED_MEM_CMM_MFC_POST

// 2008.03.24 Modified for tv-out by hyunkyung
//#define CONFIG_RESERVED_MEM_CMM_JPEG_MFC_POST_CAMERA
//#define CONFIG_RESERVED_MEM_TV_MFC_POST_CAMERA
//#define CONFIG_RESERVED_MEM_TV_CMM_JPEG_MFC_POST_CAMERA
// added by jamie
//#define CONFIG_RESERVED_MEM_CMM_POST_GPU_PMEM
#ifdef CONFIG_ANDROID_PMEM
// added by sy82.yoon(2009.04.21)
#define CONFIG_RESERVED_MEM_TV_CMM_JPEG_MFC_POST_CAMERA_GPU_PMEM
#else	
#define CONFIG_RESERVED_MEM_TV_CMM_JPEG_MFC_POST_CAMERA
#endif

#if defined(CONFIG_RESERVED_MEM_JPEG)
#define JPEG_RESERVED_MEM_START		0x57800000

#elif defined(CONFIG_RESERVED_MEM_JPEG_POST)	
#define JPEG_RESERVED_MEM_START		0x57000000
#define POST_RESERVED_MEM_START		0x57800000

#elif defined(CONFIG_RESERVED_MEM_MFC)			
#define MFC_RESERVED_MEM_START		0x57A00000

#elif defined(CONFIG_RESERVED_MEM_MFC_POST)
#define MFC_RESERVED_MEM_START		0x57200000
#define POST_RESERVED_MEM_START		0x57800000

#elif defined(CONFIG_RESERVED_MEM_JPEG_MFC_POST)
#define JPEG_RESERVED_MEM_START		0x56A00000
#define MFC_RESERVED_MEM_START		0x57200000
#define POST_RESERVED_MEM_START		0x57800000

#elif defined(CONFIG_RESERVED_MEM_JPEG_CAMERA)
#define JPEG_RESERVED_MEM_START		0x56900000

#elif defined(CONFIG_RESERVED_MEM_JPEG_POST_CAMERA)
#define JPEG_RESERVED_MEM_START		0x56100000
#define POST_RESERVED_MEM_START		0x56900000

#elif defined(CONFIG_RESERVED_MEM_MFC_CAMERA)
#define MFC_RESERVED_MEM_START		0x56B00000

#elif defined(CONFIG_RESERVED_MEM_MFC_POST_CAMERA)
#define MFC_RESERVED_MEM_START		0x56300000
#define POST_RESERVED_MEM_START		0x56900000

#elif defined(CONFIG_RESERVED_MEM_JPEG_MFC_POST_CAMERA)
#define JPEG_RESERVED_MEM_START		0x55B00000
#define MFC_RESERVED_MEM_START		0x56300000
#define POST_RESERVED_MEM_START		0x56900000

#elif defined (CONFIG_RESERVED_MEM_CMM_MFC_POST)
#define CMM_RESERVED_MEM_START		0x56A00000
#define MFC_RESERVED_MEM_START		0x57200000
#define POST_RESERVED_MEM_START		0x57800000

#elif defined (CONFIG_RESERVED_MEM_CMM_JPEG_MFC_POST_CAMERA)
#define CMM_RESERVED_MEM_START		0x55300000
#define JPEG_RESERVED_MEM_START		0x55B00000
#define MFC_RESERVED_MEM_START		0x56300000
#define POST_RESERVED_MEM_START		0x56900000

#elif defined (CONFIG_RESERVED_MEM_TV_MFC_POST_CAMERA)
#define TV_RESERVED_MEM_START		0x55B00000
#define MFC_RESERVED_MEM_START		0x56300000
#define POST_RESERVED_MEM_START		0x56900000

// 2008.03.24 add for tvout by hyunkyung
#elif defined (CONFIG_RESERVED_MEM_TV_CMM_JPEG_MFC_POST_CAMERA)
#define TV_RESERVED_MEM_START           0x55700000
#define CMM_RESERVED_MEM_START          0x55800000
#define JPEG_RESERVED_MEM_START         0x56000000
#define MFC_RESERVED_MEM_START          0x56800000
#define POST_RESERVED_MEM_START         0x56e00000
#define CAMERA_RESERVED_MEM_START    		0x57600000

// added by sy82.yoon(2009.04.21)
/* TV : 1MB
 * CMM : 8MB
 * JPEG : 8MB
 * MFC : 6MB
 * Post : 1MB
 * CAMERA : 10MB
 * GPU : 2MB
 * PMEM : 16MB
 */
#elif defined (CONFIG_RESERVED_MEM_TV_CMM_JPEG_MFC_POST_CAMERA_GPU_PMEM)
#define	TV_RESERVED_MEM_START			(DRAM_END_ADDR - RESERVED_MEM_TVOUT + DRAM_OFFSET)
#define CMM_RESERVED_MEM_START    (TV_RESERVED_MEM_START - RESERVED_MEM_CMM)
#define JPEG_RESERVED_MEM_START   (CMM_RESERVED_MEM_START - RESERVED_MEM_JPEG)
#define MFC_RESERVED_MEM_START    (JPEG_RESERVED_MEM_START - RESERVED_MEM_MFC)
#define POST_RESERVED_MEM_START   (MFC_RESERVED_MEM_START - RESERVED_MEM_POST)
#define	CAMERA_RESERVED_MEM_START	(POST_RESERVED_MEM_START - RESERVED_MEM_CAMERA)
#define GPU_RESERVED_MEM_START    (CAMERA_RESERVED_MEM_START - RESERVED_MEM_GPU)
#define PMEM_RESERVED_MEM_START   (GPU_RESERVED_MEM_START - RESERVED_MEM_PMEM)

#elif defined (CONFIG_RESERVED_MEM_CMM_POST_GPU_PMEM)
// added by jamie in order to load 3D on 128M (2009.04.14)
/* MFC, JPEG, TV, Camera : 0 MB
 * Post   : 1 MB
 * CMM    : 2 MB
 * GPU    : 2 MB
 * PMEM   : 8 MB
*/
#define POST_RESERVED_MEM_START   0x57200000
#define CMM_RESERVED_MEM_START    0x57400000
#define GPU_RESERVED_MEM_START    0x57600000
#define PMEM_RESERVED_MEM_START   0x57800000
#endif

#endif /* _ASM_ARM_ARCH_RESERVED_MEM_H */


