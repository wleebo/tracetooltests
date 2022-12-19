#include "vulkan_common.h"
#include <inttypes.h>

static int fence_variant = 0;
static bool ugly_exit = false;
static int vulkan_variant = 1;
static vulkan_req_t reqs;

static void show_usage()
{
	printf("-x/--ugly-exit         Exit without cleanup\n");
	printf("-f/--fence-variant N   Set fence variant (default %d)\n", fence_variant);
	printf("\t0 - normal run\n");
	printf("\t1 - expect induced fence delay\n");
	printf("-V/--vulkan-variant N  Set Vulkan variant (default %d)\n", vulkan_variant);
	printf("\t0 - Vulkan 1.0\n");
	printf("\t1 - Vulkan 1.1\n");
	printf("\t2 - Vulkan 1.2\n");
	printf("\t3 - Vulkan 1.3\n");
}

static bool test_cmdopt(int& i, int argc, char** argv, vulkan_req_t& reqs)
{
	if (match(argv[i], "-x", "--ugly-exit"))
	{
		ugly_exit = true;
		return true;
	}
	else if (match(argv[i], "-f", "--fence-variant"))
	{
		fence_variant = get_arg(argv, ++i, argc);
		return (fence_variant >= 0 && fence_variant <= 1);
	}
	else if (match(argv[i], "-V", "--vulkan-variant"))
	{
		vulkan_variant = get_arg(argv, ++i, argc);
		if (vulkan_variant == 0) reqs.apiVersion = VK_API_VERSION_1_0;
		else if (vulkan_variant == 1) reqs.apiVersion = VK_API_VERSION_1_1;
		else if (vulkan_variant == 2) reqs.apiVersion = VK_API_VERSION_1_2;
		else if (vulkan_variant == 3) reqs.apiVersion = VK_API_VERSION_1_3;
		return (vulkan_variant >= 0 && vulkan_variant <= 3);
	}
	return false;
}

int main(int argc, char** argv)
{
	reqs.usage = show_usage;
	reqs.cmdopt = test_cmdopt;
	vulkan_setup_t vulkan = test_init(argc, argv, "vulkan_general", reqs);
	VkResult r;

	// Test vkEnumerateInstanceVersion

	if (vulkan_variant >= 1)
	{
		uint32_t pApiVersion = 0;
		r = vkEnumerateInstanceVersion(&pApiVersion); // new in Vulkan 1.1
		assert(r == VK_SUCCESS);
		assert(pApiVersion > 0);
		printf("vkEnumerateInstanceVersion says Vulkan %d.%d.%d)\n", VK_VERSION_MAJOR(pApiVersion),
                       VK_VERSION_MINOR(pApiVersion), VK_VERSION_PATCH(pApiVersion));
	}

	// Test tool interference in function lookups

	PFN_vkVoidFunction badptr = vkGetInstanceProcAddr(nullptr, "vkNonsense");
	assert(!badptr);
	badptr = vkGetInstanceProcAddr(vulkan.instance, "vkNonsense");
	assert(!badptr);
	badptr = vkGetDeviceProcAddr(vulkan.device, "vkNonsense");
	assert(!badptr);
	PFN_vkVoidFunction goodptr = vkGetInstanceProcAddr(nullptr, "vkCreateInstance");
	assert(goodptr);
	goodptr = vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties");
	assert(goodptr);
	goodptr = vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
	assert(goodptr);
	goodptr = vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceExtensionProperties");
	assert(goodptr);
	if (vulkan_variant >= 2)
	{
		goodptr = vkGetInstanceProcAddr(nullptr, "vkGetInstanceProcAddr"); // Valid starting with Vulkan 1.2
		assert(goodptr);
	}
	goodptr = vkGetInstanceProcAddr(vulkan.instance, "vkGetInstanceProcAddr");
	assert(goodptr);

	if (vulkan_variant >= 1)
	{
		uint32_t devgrpcount = 0;
		r = vkEnumeratePhysicalDeviceGroups(vulkan.instance, &devgrpcount, nullptr);
		std::vector<VkPhysicalDeviceGroupProperties> devgrps(devgrpcount);
		for (auto& v : devgrps) { v.pNext = nullptr; v.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES; }
		printf("Found %u physical device groups:\n", devgrpcount);
		r = vkEnumeratePhysicalDeviceGroups(vulkan.instance, &devgrpcount, devgrps.data());
		for (auto& v : devgrps)
		{
			printf("\t%u devices (subsetAllocation=%s):", v.physicalDeviceCount, v.subsetAllocation ? "true" : "false");
			for (unsigned i = 0; i < v.physicalDeviceCount; i++) printf(" 0x%" PRIx64 ",", (uint64_t)v.physicalDevices[i]);
			printf("\n");
		}
	}

	// Test tool interference in fence handling

	VkFence fence1;
	VkFence fence2;
	VkFenceCreateInfo fence_create_info = {};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	r = vkCreateFence(vulkan.device, &fence_create_info, NULL, &fence1);
	check(r);
	r = vkCreateFence(vulkan.device, &fence_create_info, NULL, &fence2);
	check(r);

	VkQueue queue;
	vkGetDeviceQueue(vulkan.device, 0, 0, &queue);
	r = vkQueueSubmit(queue, 0, nullptr, fence1); // easiest way to signal a fence...
	check(r);

	r = vkWaitForFences(vulkan.device, 1, &fence1, VK_TRUE, UINT32_MAX - 1);
	if (fence_variant == 0) assert(r == VK_SUCCESS);
	else assert(r == VK_TIMEOUT);

	std::vector<VkFence> fences = { fence1, fence2 }; // one signaled, one unsignaled
	r = vkWaitForFences(vulkan.device, 2, fences.data(), VK_TRUE, 10);
	assert(r == VK_TIMEOUT);

	r = vkGetFenceStatus(vulkan.device, fence1);
	if (fence_variant == 0) assert(r == VK_SUCCESS);
	else assert(r == VK_NOT_READY);

	r = vkGetFenceStatus(vulkan.device, fence2);
	assert(r == VK_NOT_READY);

	r = vkResetFences(vulkan.device, 2, fences.data());
	check(r);
	r = vkGetFenceStatus(vulkan.device, fence1);
	assert(r == VK_NOT_READY);

	// Test private data
	bool private_data_support = false;
	if (vulkan_variant >= 3)
	{
		VkPhysicalDevicePrivateDataFeatures pFeat = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES, nullptr };
		VkPhysicalDeviceFeatures2 feat2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &pFeat };
		vkGetPhysicalDeviceFeatures2(vulkan.physical, &feat2);
		private_data_support = pFeat.privateData;
		if (!private_data_support) ILOG("Private data feature not supported!");
	}
	if (vulkan_variant >= 3 && private_data_support)
	{
		VkPrivateDataSlotCreateInfo pdinfo = { VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO, nullptr, 0 };
		VkPrivateDataSlot pdslot;
		r = vkCreatePrivateDataSlot(vulkan.device, &pdinfo, nullptr, &pdslot);
		check(r);
		r = vkSetPrivateData(vulkan.device, VK_OBJECT_TYPE_FENCE, (uint64_t)fence1, pdslot, 1234);
		check(r);
		uint64_t pData = 0;
		vkGetPrivateData(vulkan.device, VK_OBJECT_TYPE_FENCE, (uint64_t)fence1, pdslot, &pData);
		assert(pData == 1234);
		vkDestroyPrivateDataSlot(vulkan.device, pdslot, nullptr);
		// TBD test device pre-allocate private data using VkDevicePrivateDataCreateInfo + VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO
	}

	// Test valid null destroy commands

	vkDestroyInstance(VK_NULL_HANDLE, nullptr);
	vkDestroyDevice(VK_NULL_HANDLE, nullptr);
	vkDestroyFence(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroySemaphore(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyEvent(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyQueryPool(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyBuffer(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyBufferView(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyImage(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyImageView(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyShaderModule(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyPipelineCache(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyPipeline(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyPipelineLayout(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroySampler(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyDescriptorSetLayout(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyDescriptorPool(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyFramebuffer(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyRenderPass(vulkan.device, VK_NULL_HANDLE, nullptr);
	vkDestroyCommandPool(vulkan.device, VK_NULL_HANDLE, nullptr);
	if (vulkan_variant >= 1)
	{
		vkDestroySamplerYcbcrConversion(vulkan.device, VK_NULL_HANDLE, nullptr);
		vkDestroyDescriptorUpdateTemplate(vulkan.device, VK_NULL_HANDLE, nullptr);
	}
	if (vulkan_variant >= 3)
	{
		vkDestroyPrivateDataSlot(vulkan.device, VK_NULL_HANDLE, nullptr);
	}

	// Optionally test ugly exit

	if (!ugly_exit)
	{
		vkDestroyFence(vulkan.device, fence1, nullptr);
		vkDestroyFence(vulkan.device, fence2, nullptr);
		test_done(vulkan);
	}

	return 0;
}
