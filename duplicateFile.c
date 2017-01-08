#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main{int argc, char* argv}
{
  path disk (argv[1]);
  path directory (argv[2]);
  path file (argv[3]);
  int fd_to, fd_from;
  char buf[4096];
  ssize_t nread;
  
    if FS_Boot(disk) = 0{
      
      printf("Disk booted\n");
      if Dir_Create(directory) = 0{
        
        printf("Directory %s created", directory);
        File_Create(file)
        if(file){
          
          printf("File %s created", file)
          dirp = opendir(directory)
          chdir(directory)
          if(dirp){
            
            while Dir_Read != NULL{
              
              while (nread = read(fd_from, buf, sizeof buf), nread > 0){
              char *out_ptr = buf;
              ssize_t nwritten;

              do {
                  nwritten = write(fd_to, out_ptr, nread);

                  if (nwritten >= 0){

                  nread -= nwritten;
                  out_ptr += nwritten;}

              else if (errno != EINTR){

                goto out_error;
              }
          } 
          while (nread > 0);}

          if (nread == 0){

          if (close(fd_to) < 0){

            fd_to = -1;
            goto out_error;}

            close(fd_from);
                }
              }
              File_Close(file)
            }
            Dir_Unlink(directory)
        }FS_Sync()
          
    }return 0
}
  