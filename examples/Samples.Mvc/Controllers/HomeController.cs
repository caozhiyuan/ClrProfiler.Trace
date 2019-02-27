using System;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;

namespace Samples.Mvc.Controllers
{
    [Route("home")]
    public class HomeController : Controller
    {
        private static readonly HttpClient HttpClient = new HttpClient();

        private readonly ILogger _logger;

        public HomeController(ILogger<HomeController> logger)
        {
            _logger = logger;
        }

        [HttpGet]
        [Route("test1")]
        public async Task<IActionResult> Test1()
        {
            var str = await HttpClient.GetAsync("http://127.0.0.1:18787/home/Test2");
            return Json(str);
        }

        [HttpGet]
        [Route("test2")]
        public async Task<IActionResult> Test2()
        {
            await Task.Delay(100);
            return Json("1");
        }
    }
}
