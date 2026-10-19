# MM3's authored fog shaders. The runtime owns no copy of these rules or HLSL.
function(mm3_render_fog_shaders target backend)
    get_filename_component(_mm3 "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(_shader "${_mm3}/src/mods/shaders/distance_fog.hlsl")
    set(_gen "${CMAKE_CURRENT_BINARY_DIR}/${target}_mm3_fog")
    file(MAKE_DIRECTORY "${_gen}")
    target_include_directories(${target} PRIVATE "${_gen}")
    if(backend STREQUAL "SDL3_GPU")
        set(_formats spirv)
        if(WIN32)
            list(APPEND _formats dxil)
        endif()
        foreach(_stage vs ps)
            string(TOUPPER "${_stage}" _STAGE)
            foreach(_format IN LISTS _formats)
                set(_out "${_gen}/mm3_fog_${_stage}.${_format}.h")
                set(_flags)
                if(_format STREQUAL "spirv")
                    set(_flags -spirv -fspv-target-env=vulkan1.0 -fvk-use-dx-layout)
                endif()
                add_custom_command(OUTPUT "${_out}"
                    COMMAND "${XBOXRECOMP_DXC}" -nologo -HV 2018 -Wno-ignored-attributes
                        -T ${_stage}_6_0 -E fog_${_stage} -D SDLGPU -D STAGE_${_STAGE}
                        -I "${XBOXRECOMP_DIR}/src/video/shaders" ${_flags}
                        -Fh "${_out}" -Vn mm3_fog_${_stage}_${_format} "${_shader}"
                    DEPENDS "${_shader}" "${XBOXRECOMP_DIR}/src/video/shaders/nv2a_bindings.hlsli"
                    VERBATIM)
                target_sources(${target} PRIVATE "${_out}")
            endforeach()
        endforeach()
    else()
        set(_out "${_gen}/mm3_distance_fog.inc")
        add_custom_command(OUTPUT "${_out}"
            COMMAND "${CMAKE_COMMAND}" -DNAME=s_native_fog_hlsl -DOUT=${_out}
                -DINPUTS=${_shader} -P "${XBOXRECOMP_DIR}/cmake/EmbedText.cmake"
            DEPENDS "${_shader}" "${XBOXRECOMP_DIR}/cmake/EmbedText.cmake" VERBATIM)
        target_sources(${target} PRIVATE "${_out}")
    endif()
endfunction()
