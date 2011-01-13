
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

#ifndef APPLESEED_RENDERER_KERNEL_LIGHTING_PATHTRACER_H
#define APPLESEED_RENDERER_KERNEL_LIGHTING_PATHTRACER_H

// appleseed.renderer headers.
#include "renderer/global/global.h"
#include "renderer/kernel/intersection/intersector.h"
#include "renderer/kernel/shading/shadingcontext.h"
#include "renderer/kernel/shading/shadingpoint.h"
#include "renderer/kernel/shading/shadingray.h"
#include "renderer/modeling/bsdf/bsdf.h"
#include "renderer/modeling/edf/edf.h"
#include "renderer/modeling/input/inputevaluator.h"
#include "renderer/modeling/material/material.h"
#include "renderer/modeling/surfaceshader/surfaceshader.h"

// appleseed.foundation headers.
#include "foundation/math/basis.h"
#include "foundation/math/rr.h"
#include "foundation/utility/string.h"

// Standard headers.
#include <algorithm>

// Forward declarations.
namespace renderer  { class TextureCache; }

namespace renderer
{

//
// A generic path tracer.
//

template <
    typename    PathVertexVisitor,
    int         ScatteringModesMask,
    bool        Adjoint
>
class PathTracer
{
  public:
    PathTracer(
        PathVertexVisitor&      vertex_visitor,
        const size_t            rr_minimum_path_length);

    size_t trace(
        SamplingContext&        sampling_context,
        const ShadingContext&   shading_context,
        const ShadingPoint&     shading_point,
        Spectrum&               radiance);          // output radiance, in W.sr^-1.m^-2

  private:
    PathVertexVisitor&          m_vertex_visitor;
    const size_t                m_rr_minimum_path_length;

    static bool has_reached_max_path_length(const size_t path_length);
};


//
// PathTracer class implementation.
//

template <
    typename    PathVertexVisitor,
    int         ScatteringModesMask,
    bool        Adjoint
>
PathTracer<PathVertexVisitor, ScatteringModesMask, Adjoint>::PathTracer(
    PathVertexVisitor&          vertex_visitor,
    const size_t                rr_minimum_path_length)
  : m_vertex_visitor(vertex_visitor)
  , m_rr_minimum_path_length(rr_minimum_path_length)
{
}

template <
    typename    PathVertexVisitor,
    int         ScatteringModesMask,
    bool        Adjoint
>
size_t PathTracer<PathVertexVisitor, ScatteringModesMask, Adjoint>::trace(
    SamplingContext&            sampling_context,
    const ShadingContext&       shading_context,
    const ShadingPoint&         shading_point,
    Spectrum&                   radiance)
{
    // Initialize path radiance.
    radiance.set(0.0f);

    // Retrieve items from the shading context.
    const Intersector& intersector = shading_context.get_intersector();
    TextureCache& texture_cache = shading_context.get_texture_cache();

    ShadingPoint shading_points[2];
    size_t shading_point_index = 0;
    const ShadingPoint* shading_point_ptr = &shading_point;

    // Trace one path.
    Spectrum throughput(1.0f);
    size_t path_length = 1;
    BSDF::Mode bsdf_mode = BSDF::Specular;
    double bsdf_prob = BSDF::DiracDelta;
    while (true)
    {
        // Limit the length of the path.
        if (has_reached_max_path_length(path_length))
            break;

        // Retrieve the ray.
        const ShadingRay& ray = shading_point_ptr->get_ray();

        // Terminate the path if the ray didn't hit anything.
        if (!shading_point_ptr->hit())
        {
            Spectrum environment_radiance;
            if (m_vertex_visitor.get_environment_radiance(
                    *shading_point_ptr,
                    normalize(-ray.m_dir),
                    environment_radiance))
            {
                // Update the path radiance.
                environment_radiance *= throughput;
                radiance += environment_radiance;
            }

            break;
        }

        // Retrieve the material at the shading point.
        const Material* material = shading_point_ptr->get_material();

        // Terminate the path if the surface has no material.
        if (material == 0)
            break;

        // Retrieve the surface shader.
        const SurfaceShader& surface_shader = material->get_surface_shader();

        // Evaluate the alpha mask at the shading point.
        Alpha alpha_mask;
        surface_shader.evaluate_alpha_mask(
            sampling_context,
            shading_context,
            *shading_point_ptr,
            alpha_mask);

        // Handle alpha masking.
        const double cutoff_prob = 1.0 - alpha_mask[0];
        if (cutoff_prob > 0.0)
        {
            if (sampling_context.next_double1() >= 1.0 - cutoff_prob)
            {
                // Construct a ray that continues in the same direction as the incoming ray.
                const ShadingRay cutoff_ray(
                    shading_point_ptr->get_point(),
                    ray.m_dir,
                    0.0f,           // ray time
                    ~0);            // ray flags

                // Trace the ray.
                shading_points[shading_point_index].clear();
                intersector.trace(
                    cutoff_ray,
                    shading_points[shading_point_index],
                    shading_point_ptr);

                // Update the pointers to the shading points.
                shading_point_ptr = &shading_points[shading_point_index];
                shading_point_index = 1 - shading_point_index;

                continue;
            }
        }

        // Retrieve the BSDF.
        // Terminate the path if the material has no BSDF.
        const BSDF* bsdf = material->get_bsdf();
        if (bsdf == 0)
            break;

        // Evaluate the input values of the BSDF.
        InputEvaluator bsdf_input_evaluator(texture_cache);
        const void* bsdf_data =
            bsdf_input_evaluator.evaluate(
                bsdf->get_inputs(),
                shading_point_ptr->get_input_params());

        // Compute the outgoing direction.
        const foundation::Vector3d outgoing = normalize(-ray.m_dir);

        // Compute the contribution of this vertex to the path radiance.
        Spectrum vertex_radiance;
        m_vertex_visitor.get_vertex_radiance(
            sampling_context,
            *shading_point_ptr,
            outgoing,
            bsdf,
            bsdf_data,
            bsdf_mode,
            bsdf_prob,
            vertex_radiance);

        // Update the path radiance.
        vertex_radiance *= throughput;
        radiance += vertex_radiance;

        // Generate a uniform sample in [0,1)^4.
        sampling_context = sampling_context.split(4, 1);
        const foundation::Vector4d s = sampling_context.next_vector2<4>();

        // Sample the BSDF.
        foundation::Vector3d incoming;
        Spectrum bsdf_value;
        bsdf->sample(
            bsdf_data,
            shading_point_ptr->get_geometric_normal(),
            shading_point_ptr->get_shading_basis(),
            foundation::Vector3d(s[0], s[1], s[2]),
            outgoing,
            incoming,
            bsdf_value,
            bsdf_prob,
            bsdf_mode);

        // Terminate the path if this scattering mode is not accepted.
        if (!(bsdf_mode & ScatteringModesMask))
            break;

        // Multiply the BSDF value by cos(theta).
        assert(foundation::is_normalized(incoming));
        const double cos_in =
            std::abs(
                foundation::dot(
                    incoming,
                    shading_point_ptr->get_shading_normal()));
        bsdf_value *= static_cast<float>(cos_in);

        // Update the path throughput.
        throughput *= bsdf_value;

        // Use Russian Roulette to cut the path without introducing bias.
        if (path_length >= m_rr_minimum_path_length)
        {
            const double scattering_prob =
                std::min(
                    static_cast<double>(foundation::max_value(bsdf_value)),
                    1.0);

            if (!foundation::pass_rr(scattering_prob, s[3]))
                break;

            assert(scattering_prob > 0.0);
            throughput /= static_cast<float>(scattering_prob);
        }

        ++path_length;

        // Construct the scattered ray.
        const ShadingRay scattered_ray(
            shading_point_ptr->get_point(),
            incoming,
            0.0f,           // ray time
            ~0);            // ray flags

        // Trace the ray.
        shading_points[shading_point_index].clear();
        intersector.trace(
            scattered_ray,
            shading_points[shading_point_index],
            shading_point_ptr);

        // Update the pointers to the shading points.
        shading_point_ptr = &shading_points[shading_point_index];
        shading_point_index = 1 - shading_point_index;
    }

    return path_length;
}

template <
    typename    PathVertexVisitor,
    int         ScatteringModesMask,
    bool        Adjoint
>
inline
bool PathTracer<PathVertexVisitor, ScatteringModesMask, Adjoint>::has_reached_max_path_length(const size_t path_length)
{
    const size_t MaxPathLength = 10000;

    if (path_length < MaxPathLength)
        return false;

    RENDERER_LOG_WARNING(
        "reached path length limit (%s), terminating path",
        foundation::pretty_int(path_length).c_str());

    return true;
}

}       // namespace renderer

#endif  // !APPLESEED_RENDERER_KERNEL_LIGHTING_PATHTRACER_H
