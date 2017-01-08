{
  path disk (argv[1]);
  path directory (argv[2]);
  path file (argv[3]);
  
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
              
              rename(file, ".%s")
              printf("File hidden")
                }
                
              }
              File_Close(file)
            }
            Dir_Unlink(directory)
        }FS_Sync()
          
    }return 0
}
  