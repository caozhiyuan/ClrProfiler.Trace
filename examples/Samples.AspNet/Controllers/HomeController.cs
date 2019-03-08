using System;
using System.Data.SqlClient;
using System.Diagnostics;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using System.Web.Http;
using Dapper;
using MySql.Data.MySqlClient;
using StackExchange.Redis;

namespace Samples.AspNet.Controllers
{
    public class HomeController : ApiController
    {
        private static readonly HttpClient HttpClient = new HttpClient();

        //http://localhost:40965/home/
        [HttpGet]
        [Route("home")]
        public async Task<IHttpActionResult> Index()
        {
            await HttpClient.GetAsync("https://www.bing.com");
            _ = Task.Run(async () =>
            {
                await HttpClient.GetAsync("http://localhost:40965/test5");
            });
            return Json(await HttpClient.GetStringAsync("http://localhost:40965/test5"));
        }

        [HttpGet]
        [Route("test1")]
        public async Task<IHttpActionResult> Test1()
        {
            var str = await HttpClient.GetAsync("http://localhost:40965/test2?test=1");


            await HttpClient.GetAsync("http://localhost:40965/test5");

            return Json(str);
        }

        [HttpGet]
        [Route("test6")]
        public IHttpActionResult Test6()
        {
            return Json(Environment.GetEnvironmentVariables());
        }

        [HttpGet]
        [Route("test5")]
        public IHttpActionResult Test5()
        {
            var assemblies = AppDomain.CurrentDomain.GetAssemblies();
            return Json(assemblies.ToList().Select(n => new { Name = n.FullName }));
        }

        [HttpGet]
        [Route("test2")]
        public async Task<IHttpActionResult> Test2(string test)
        {
            Stopwatch sw = new Stopwatch();
            sw.Start();

            using (var connection = new SqlConnection($"Data Source=.;Initial Catalog=tempdb;Integrated Security=True"))
            {
                await connection.OpenAsync();

                await connection.ExecuteScalarAsync("SELECT 1");

                await HttpClient.GetAsync("http://localhost:40965/test5");

                await connection.ExecuteReaderAsync("SELECT 1");
            }

            sw.Stop();
            return Json(sw.ElapsedMilliseconds);
        }

        [HttpGet]
        [Route("test3")]

        public async Task<IHttpActionResult> Test3(string test)
        {
            Stopwatch sw = new Stopwatch();
            sw.Start();

            using (var connection = new MySqlConnection($"User ID=root;Password=123456;Host=127.0.0.1;Port=3306;Database=test;Pooling=true;Min Pool Size=0;Max Pool Size=100;SslMode=None"))
            {
                connection.Open();

                connection.ExecuteScalar("SELECT 1");

                await HttpClient.GetAsync("http://localhost:40965/test5");

                await connection.ExecuteReaderAsync("SELECT 1");
            }

            sw.Stop();
            return Json(sw.ElapsedMilliseconds);
        }

        private static readonly Lazy<IDatabase> RedisDb = new Lazy<IDatabase>(ValueFactory);

        private static IDatabase ValueFactory()
        {
            var redis = ConnectionMultiplexer.Connect("localhost,allowAdmin=true");
            return redis.GetDatabase(0);
        }

        [HttpGet]
        [Route("test4")]

        public async Task<IHttpActionResult> Test4()
        {
            Stopwatch sw = new Stopwatch();
            sw.Start();

            var prefix = "StackExchange.Redis.";

            await RedisDb.Value.StringSetAsync($"{prefix}INCR", "0");

            await HttpClient.GetAsync("http://localhost:40965/test5");

            await RedisDb.Value.StringGetAsync($"{prefix}INCR");

            sw.Stop();
            return Json(sw.ElapsedMilliseconds);
        }
    }
}
