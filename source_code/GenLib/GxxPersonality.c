
 #include "../Include/Global.h" 
 // extern "C" void __gxx_personality_v0();

#if 0
 void __gxx_personality_v0()
 {
   printf( "gxx_personality called!?!" );	
 } 
#endif

void Prn( char *frm , ... )
{
  char szBuf[ 255 ];
  char* pszBufPos = szBuf;
  
  va_list val;

  va_start  ( val , frm );
  vsprintf( pszBufPos , frm , val );

  // printf( "%s\n" , szBuf );
  printf( "{pid = %d , ppid = %d } , %s\n", getpid() , getppid() , szBuf );
  fflush(stdout);

} 

