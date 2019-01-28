using System;
using System.Data.SqlClient;
using System.Net.Http;
using System.Threading.Tasks;
using Dapper;
using MySql.Data.MySqlClient;
using StackExchange.Redis;

namespace Samples.NetCore
{
    class Program
    {
        private static readonly HttpClient HttpClient = new HttpClient();

        static void Main(string[] args)
        {

            Run().GetAwaiter().GetResult();

            Console.ReadLine();
        }

        private static async Task Run()
        {
            Program program = new Program();

            await program.RunStackExchange("StackExchange");

            await program.RunGetBing();

            await program.RunSqlClient();

            await program.RunMySqlClient();
        }

        private async Task<string> RunGetBing()
        {
            return await HttpClient.GetStringAsync("https://cn.bing.com/").ConfigureAwait(false);
        }

        private async Task RunSqlClient()
        {
            using (var connection = new SqlConnection($"Data Source=.;Initial Catalog=tempdb;Integrated Security=True"))
            {
                await connection.OpenAsync();

                await connection.ExecuteScalarAsync("SELECT 1");

                await connection.ExecuteReaderAsync("SELECT 1");
            }
        }

        private async Task RunMySqlClient()
        {
            try
            {
                using (var connection = new MySqlConnection($"User ID=root;Password=123456;Host=127.0.0.1;Port=3306;Database=test;Pooling=true;Min Pool Size=0;Max Pool Size=100;SslMode=None"))
                {
                    connection.Open();

                    connection.ExecuteScalar("SELECT 1");

                    await connection.ExecuteReaderAsync("SELECT 1");
                }
            }
            catch (Exception e)
            {
                Console.WriteLine(e);
                throw;
            }
        }

        private async Task RunStackExchange(string prefix)
        {
            prefix += "StackExchange.Redis.";

            Console.WriteLine($"Testing StackExchange.Redis {prefix}");
            using (var redis = ConnectionMultiplexer.Connect("localhost,allowAdmin=true"))
            {
                var db = redis.GetDatabase(0);

                await db.StringSetAsync($"{prefix}INCR", "0");

                db.StringSet($"{prefix}INCR", DateTime.Now.ToLongDateString());

                var c = db.StringGet($"{prefix}INCR");

                Console.WriteLine(c);
            }
        }
    }
}
