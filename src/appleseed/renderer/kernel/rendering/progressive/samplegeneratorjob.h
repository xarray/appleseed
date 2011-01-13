
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2011 Francois Beaune
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef APPLESEED_RENDERER_KERNEL_RENDERING_PROGRESSIVE_SAMPLEGENERATORJOB_H
#define APPLESEED_RENDERER_KERNEL_RENDERING_PROGRESSIVE_SAMPLEGENERATORJOB_H

// appleseed.renderer headers.
#include "renderer/global/global.h"
#include "renderer/kernel/rendering/progressive/sample.h"

// appleseed.foundation headers.
#include "foundation/utility/job.h"

// Standard headers.
#include <vector>

// Forward declarations.
namespace renderer  { class Frame; }
namespace renderer  { class ITileCallback; }
namespace renderer  { class ProgressiveFrameBuffer; }
namespace renderer  { class SampleGenerator; }

namespace renderer
{

class SampleGeneratorJob
  : public foundation::IJob
{
  public:
    // Constructor.
    SampleGeneratorJob(
        Frame&                  frame,
        ProgressiveFrameBuffer& framebuffer,
        SampleGenerator&        sample_generator,
        ITileCallback*          tile_callback,
        foundation::JobQueue&   job_queue,
        const size_t            job_index,
        const size_t            job_count,
        const size_t            pass);

    // Execute the job.
    virtual void execute(const size_t thread_index);

  private:
    Frame&                      m_frame;
    ProgressiveFrameBuffer&     m_framebuffer;
    SampleGenerator&            m_sample_generator;
    ITileCallback*              m_tile_callback;
    foundation::JobQueue&       m_job_queue;
    const size_t                m_job_index;
    const size_t                m_job_count;
    const size_t                m_pass;

    std::vector<Sample>         m_samples;

    size_t get_sample_count() const;

    void render();
};

}       // namespace renderer

#endif  // !APPLESEED_RENDERER_KERNEL_RENDERING_PROGRESSIVE_SAMPLEGENERATORJOB_H
