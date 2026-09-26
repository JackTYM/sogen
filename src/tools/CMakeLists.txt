if(WIN)
  add_subdirectory(dump-apiset)
endif()

add_subdirectory(sandbox)

if(WIN)
  add_subdirectory(dump-actctx-blob)
endif()
