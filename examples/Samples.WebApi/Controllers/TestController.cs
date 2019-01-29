using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using System;
using StackExchange.Redis;

namespace Samples.WebApi.Controllers
{
    [Route("test")]
    public class TestController : Controller
    {
        private readonly ILogger _logger;

        public TestController(ILogger<TestController> logger)
        {
            _logger = logger;
        }

        [HttpGet]
        [Route("index")]
        public async Task<IActionResult> Index()
        {
            var prefix = "StackExchange.Redis.";

            using (var redis = ConnectionMultiplexer.Connect("localhost,allowAdmin=true"))
            {
                var db = redis.GetDatabase(0);

                await db.StringSetAsync($"{prefix}INCR", "0");

                db.StringSet($"{prefix}INCR", DateTime.Now.ToLongDateString());

                return Json(db.StringGet($"{prefix}INCR").ToString());
            }
        }
    }
}
