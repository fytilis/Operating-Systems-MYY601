//THEOFANIS TOMPOLIS 4855
//MARINOS ARISTIDOU 5397
//ATHANASIOS FYTILIS 5381

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <lkl.h>
#include <lkl_host.h>
#if !defined(__MSYS__) && !defined(__MINGW32__)
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#else
#include <windows.h>
#endif

#include <lkl/lkl_fat_log.h>
#include "test.h"
#include "cla.h"
#include <lkl/asm/syscalls.h> // Για την lkl_sys_mkdir
#include <lkl/asm/stat.h>     // Για τις σταθερές mode (π.χ. S_IRWXU)
#include <lkl/asm/fcntl.h>   // Για τα LKL_O_... και LKL_AT_FDCWD
#include <lkl/asm/stat.h>    // Για το mode (αν και βάλαμε 0644) 

static struct {
	int printk;
	const char *disk;
	const char *fstype;
	int partition;
} cla;

struct cl_arg args[] = {
	{"disk", 'd', "disk file to use", 1, CL_ARG_STR, &cla.disk},
	{"partition", 'P', "partition to mount", 1, CL_ARG_INT, &cla.partition},
	{"type", 't', "filesystem type", 1, CL_ARG_STR, &cla.fstype},
	{0},
};


static struct lkl_disk disk;
static int disk_id = -1;

int lkl_test_disk_add(void)
{
#if defined(__MSYS__) || defined(__MINGW32__)
	disk.handle = CreateFile(cla.disk, GENERIC_READ | GENERIC_WRITE,
			       0, NULL, OPEN_EXISTING, 0, NULL);
	if (!disk.handle)
#else
	disk.fd = open(cla.disk, O_RDWR);
	if (disk.fd < 0)
#endif
		goto out_unlink;

	disk.ops = NULL;

	disk_id = lkl_disk_add(&disk);
	if (disk_id < 0)
		goto out_close;

	goto out;

out_close:
#if defined(__MSYS__) || defined(__MINGW32__)
	CloseHandle(disk.handle);
#else
	close(disk.fd);
#endif

out_unlink:
#if defined(__MSYS__) || defined(__MINGW32__)
	DeleteFile(cla.disk);
#else
	unlink(cla.disk);
#endif

out:
	lkl_test_logf("disk fd/handle %x disk_id %d", disk.fd, disk_id);

	if (disk_id >= 0)
		return TEST_SUCCESS;

	return TEST_FAILURE;
}

int lkl_test_disk_remove(void)
{
	int ret;

	ret = lkl_disk_remove(disk);

#if defined(__MSYS__) || defined(__MINGW32__)
	CloseHandle(disk.handle);
#else
	close(disk.fd);
#endif

	if (ret == 0)
		return TEST_SUCCESS;

	return TEST_FAILURE;
}


static char mnt_point[32];

LKL_TEST_CALL(mount_dev, lkl_mount_dev, 0, disk_id, cla.partition, cla.fstype,
	      0, NULL, mnt_point, sizeof(mnt_point))

static int lkl_test_umount_dev(void)
{
	long ret, ret2;

	ret = lkl_sys_chdir("/");

	ret2 = lkl_umount_dev(disk_id, cla.partition, 0, 1000);

	lkl_test_logf("%ld %ld", ret, ret2);

	if (!ret && !ret2)
		return TEST_SUCCESS;

	return TEST_FAILURE;
}

struct lkl_dir *dir;

static int lkl_test_opendir(void)
{
	int err;

	dir = lkl_opendir(mnt_point, &err);

	lkl_test_logf("lkl_opendir(%s) = %d %s\n", mnt_point, err,
		      lkl_strerror(err));

	if (err == 0)
		return TEST_SUCCESS;

	return TEST_FAILURE;
}

static int lkl_test_readdir(void)
{
	struct lkl_linux_dirent64 *de = lkl_readdir(dir);
	int wr = 0;

	while (de) {
		wr += lkl_test_logf("%s ", de->d_name);
		if (wr >= 70) {
			lkl_test_logf("\n");
			wr = 0;
			break;
		}
		de = lkl_readdir(dir);
	}

	if (lkl_errdir(dir) == 0)
		return TEST_SUCCESS;

	return TEST_FAILURE;
}

// Νέα συνάρτηση για το test δημιουργίας αρχείου
static int lkl_test_create_file(void)
{
	long fd;
 	long ret = TEST_FAILURE; 
    	const char *new_file_name = "myfile.txt";
    	int flags = LKL_O_CREAT | LKL_O_WRONLY | LKL_O_TRUNC;
    	mode_t mode = 0644;

    	fd = lkl_sys_openat(LKL_AT_FDCWD, new_file_name, flags, mode);
	
    	lkl_test_logf("lkl_sys_openat(%s, flags=0x%x, mode=0%o) = %ld",
                  new_file_name, flags, mode, fd);

    	if (fd >= 0) {
		char write_buf[40000]; // Ένα buffer ~40KB 
		long bytes_to_write = sizeof(write_buf);
		long bytes_written;

		memset(write_buf, 'B', bytes_to_write); // Γέμισέ το με 'B'
		lkl_test_logf("Attempting to write %ld bytes...", bytes_to_write);
	        bytes_written = lkl_sys_write(fd, write_buf, bytes_to_write);
		lkl_test_logf("lkl_sys_write(%ld, ...) = %ld", fd, bytes_written);

		if (bytes_written != bytes_to_write) {
		     lkl_test_logf("Write operation may have failed or written less bytes.");
		}
        	long close_ret = lkl_sys_close(fd);
        	lkl_test_logf("lkl_sys_close(%ld) = %ld", fd, close_ret);
        	if (close_ret == 0) {
             		ret = TEST_SUCCESS; // Επιτυχία μόνο αν και το close πέτυχε
        	}
    	}

    	return ret;
}

LKL_TEST_CALL(closedir, lkl_closedir, 0, dir);
LKL_TEST_CALL(chdir_mnt_point, lkl_sys_chdir, 0, mnt_point);
LKL_TEST_CALL(start_kernel, lkl_start_kernel, 0, "mem=128M loglevel=8");
LKL_TEST_CALL(stop_kernel, lkl_sys_halt, 0);

struct lkl_test tests[] = {
	LKL_TEST(disk_add),
	LKL_TEST(start_kernel),
	LKL_TEST(mount_dev),
	LKL_TEST(chdir_mnt_point),
	LKL_TEST(create_file),
	LKL_TEST(opendir),
	LKL_TEST(readdir),
	LKL_TEST(closedir),
	LKL_TEST(umount_dev),
	LKL_TEST(stop_kernel),
	LKL_TEST(disk_remove),

};

int main(int argc, const char **argv)
{
	int ret;

	if (parse_args(argc, argv, args) < 0)
		return -1;

	if (!cla.disk || !cla.fstype) {
		fprintf(stderr, "disk and type parameters required.\n");
		return -1;
	}

	lkl_host_ops.print = lkl_test_log;

	lkl_init(&lkl_host_ops);
	lkl_fat_log_init();

	ret = lkl_test_run(tests, sizeof(tests)/sizeof(struct lkl_test),
			"disk %s", cla.fstype);

	lkl_fat_log_close();	
	lkl_cleanup();

	return ret;
}
