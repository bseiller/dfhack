PROJECT (embark-assistant)
# A list of source files
SET(PROJECT_SRCS
    embark-assistant.cpp
    finder_ui.cpp
    help_ui.cpp
    index.cpp
    key_position_mapper.cpp
    matcher.cpp
    overlay.cpp
    roaring.c
    screen.cpp
    survey.cpp
)
# A list of headers
SET(PROJECT_HDRS
    defs.h
    embark-assistant.h
    finder_ui.h
    help_ui.h
    index.h
    key_position_mapper.h
    matcher.h
    overlay.h
    query.h
    roaring.h
    roaring.hh
    screen.h
    survey.h
)
SET_SOURCE_FILES_PROPERTIES( ${PROJECT_HDRS} PROPERTIES HEADER_FILE_ONLY TRUE)

# mash them together (headers are marked as headers and nothing will try to compile them)
LIST(APPEND PROJECT_SRCS ${PROJECT_HDRS})

DFHACK_PLUGIN(embark-assistant ${PROJECT_SRCS})
