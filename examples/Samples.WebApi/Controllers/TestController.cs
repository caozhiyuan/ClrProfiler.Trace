using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using System;
using System.Data.SqlClient;
using System.Net.Http;
using Dapper;
using MySql.Data.MySqlClient;
using StackExchange.Redis;

namespace Samples.WebApi.Controllers
{
    [Route("test")]
    public class TestController : Controller
    {
        private static readonly HttpClient HttpClient = new HttpClient();
        private readonly ILogger _logger;

        public TestController(ILogger<TestController> logger)
        {
            _logger = logger;
        }

        [HttpGet]
        [Route("index")]
        public async Task<IActionResult> Index()
        {
            await HttpClient.GetStringAsync("https://www.baidu.com/");

            using (var connection = new SqlConnection($"Data Source=.;Initial Catalog=tempdb;Integrated Security=True"))
            {
                await connection.OpenAsync();

                await connection.ExecuteScalarAsync("SELECT 1");

                await connection.ExecuteReaderAsync("SELECT 1");
            }

            using (var connection = new MySqlConnection($"User ID=root;Password=123456;Host=127.0.0.1;Port=3306;Database=test;Pooling=true;Min Pool Size=0;Max Pool Size=100;SslMode=None"))
            {
                connection.Open();

                connection.ExecuteScalar("SELECT 1");

                await connection.ExecuteReaderAsync("SELECT 1");
            }

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
