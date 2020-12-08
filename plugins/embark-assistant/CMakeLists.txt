PROJECT (embark-assistant)
# A list of source files
SET(PROJECT_SRCS
    embark-assistant.cpp
    finder_ui.cpp
    help_ui.cpp
    incursion_processor.cpp
    index.cpp
    inorganics_information.cpp
    key_buffer_holder.cpp
    key_position_mapper.cpp
    matcher.cpp
    overlay.cpp
    query_plan_factory.cpp
    roaring.c
    screen.cpp
    survey.cpp
)
# A list of headers
SET(PROJECT_HDRS
    basic_key_buffer_holder.h
    defs.h
    embark-assistant.h
    finder_ui.h
    guarded_roaring.h
    help_ui.h
    incursion_processor.h
    index.h
    inorganics_information.h
    key_buffer_holder.h
    key_buffer_holder_interface.h
    key_position_mapper.h
    matcher.h
    overlay.h
    query.h
    query_plan_factory.h
    roaring.h
    roaring.hh
    screen.h
    survey.h
)
SET_SOURCE_FILES_PROPERTIES( ${PROJECT_HDRS} PROPERTIES HEADER_FILE_ONLY TRUE)

# mash them together (headers are marked as headers and nothing will try to compile them)
LIST(APPEND PROJECT_SRCS ${PROJECT_HDRS})

DFHACK_PLUGIN(embark-assistant ${PROJECT_SRCS})
