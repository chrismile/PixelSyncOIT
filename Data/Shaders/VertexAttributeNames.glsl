/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2018 - 2021, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Originally, the two defines below were used. They worked fine on the open-source Linux graphics drivers,
 * but caused problem on the closed-source NVIDIA drivers (for whatever reason)...
 */
//#define CONCAT_VERTEX_ATTRIBUTE_NAME(n) vertexAttribute##n
//#define vertexAttribute CONCAT_VERTEX_ATTRIBUTE_NAME(IMPORTANCE_CRITERION_INDEX)

#if IMPORTANCE_CRITERION_INDEX == 0
#define VERTEX_ATTRIBUTE vertexAttribute0
#elif IMPORTANCE_CRITERION_INDEX == 1
#define VERTEX_ATTRIBUTE vertexAttribute1
#elif IMPORTANCE_CRITERION_INDEX == 2
#define VERTEX_ATTRIBUTE vertexAttribute2
#elif IMPORTANCE_CRITERION_INDEX == 3
#define VERTEX_ATTRIBUTE vertexAttribute3
#elif IMPORTANCE_CRITERION_INDEX == 4
#define VERTEX_ATTRIBUTE vertexAttribute4
#elif IMPORTANCE_CRITERION_INDEX == 5
#define VERTEX_ATTRIBUTE vertexAttribute5
#elif IMPORTANCE_CRITERION_INDEX == 6
#define VERTEX_ATTRIBUTE vertexAttribute6
#elif IMPORTANCE_CRITERION_INDEX == 7
#define VERTEX_ATTRIBUTE vertexAttribute7
#elif IMPORTANCE_CRITERION_INDEX == 8
#define VERTEX_ATTRIBUTE vertexAttribute8
#elif IMPORTANCE_CRITERION_INDEX == 9
#define VERTEX_ATTRIBUTE vertexAttribute9
#endif
