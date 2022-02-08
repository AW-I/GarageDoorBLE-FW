set(srcs    "Core/main.c"
            "Application/a_task_handler.c")

set(incs    "Core/Inc"
            "Application/Inc")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${incs}")